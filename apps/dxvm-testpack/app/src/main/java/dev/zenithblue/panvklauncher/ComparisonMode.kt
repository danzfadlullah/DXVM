package dev.zenithblue.panvklauncher

import android.content.Context
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.File

/**
 * Optional comparison run: `system driver + DXVM` beside `PanVK + stock DXVK`.
 *
 * Off by default and never implied. The value of the app is the *system driver* measurement, so
 * a comparison is only an extra column the owner asks for — and the banner has to name both
 * drivers, because two half-reports pasted together without labels are worse than one.
 */
fun mergeComparison(runA: String, runB: String): String = buildString {
    appendLine("== Run A: system driver + DXVM ==")
    appendLine(runA.trimEnd())
    appendLine()
    appendLine("== Run B: PanVK + stock DXVK ==")
    appendLine(runB.trimEnd())
    appendLine()
}

/** What a comparison run needs beyond the default configuration. */
data class ComparisonPrereq(
    /** A usable PanVK driver's id: present *and* its library actually on disk. */
    val panvkDriverId: String?,
    /** A DXVK component that is *not* DXVM (no marker), or null when none is installed. */
    val stockDxvkDir: File?
) {
    val ready: Boolean get() = panvkDriverId != null && stockDxvkDir != null

    /** Exactly what is missing, for Run B's section — a silent skip would look like a done job. */
    fun missing(): List<String> = buildList {
        if (panvkDriverId == null) add("no usable PanVK driver (Drivers screen)")
        if (stockDxvkDir == null) add("no stock DXVK component installed (Components screen)")
    }
}

object ComparisonMode {
    /** Reads what the comparison would use, without changing anything. */
    fun prereq(ctx: Context): ComparisonPrereq {
        // A driver entry whose .so is missing cannot drive anything, so its existence on disk is
        // part of the check rather than assumed from the list entry.
        val panvk = DriverManager.getDrivers(ctx).firstOrNull { isPanVk(it) && File(it.libPath).isFile }?.id
        val stock = ContentManager.list(ctx)
            .firstOrNull { it.type == "DXVK" && DxvmComponent.versionOf(it.dir) == null }
            ?.dir
        return ComparisonPrereq(panvk, stock)
    }

    fun isPanVk(driver: Driver): Boolean =
        driver.name.contains("panvk", ignoreCase = true) ||
            driver.id.contains("panvk", ignoreCase = true) ||
            driver.libPath.contains("panvk", ignoreCase = true)

    /** Label for the D3D half when the stock DXVK is staged on PanVK. */
    const val PANVK_DXVK_LABEL = "PanVK + stock DXVK"
}

/**
 * Run B: the same two halves, but the Direct3D half runs on PanVK with the stock DXVK staged
 * next to the harness, and the Vulkan half keeps describing this process's loader.
 */
private class PanVkRunHost(
    private val delegate: AndroidTestFlowHost,
    private val panvkDriverId: String,
    private val stockDxvkDir: File
) : TestFlowHost by delegate {
    override val d3dDriverId: String get() = panvkDriverId
    override val d3dDriverLabel: String get() = ComparisonMode.PANVK_DXVK_LABEL
    override suspend fun runD3d(progress: (String) -> Unit): List<D3DSuite.ResultLine> =
        delegate.runD3d(progress, driverId = panvkDriverId, stageFrom = stockDxvkDir)
}

/**
 * Runs the full pass under the system driver with DXVM, then — only when the owner has installed
 * both prerequisites — the same two halves under PanVK with the stock DXVK, and returns both
 * reports labelled.
 */
suspend fun runComparison(ctx: Context, progress: (String) -> Unit): String = withContext(Dispatchers.IO) {
    progress("Comparison: Run A — system driver + DXVM")
    val runA = runFullPass(ctx, progress)

    val prereq = ComparisonMode.prereq(ctx)
    if (!prereq.ready) {
        progress("Comparison: Run B unavailable — ${prereq.missing().joinToString("; ")}")
        return@withContext mergeComparison(runA, unavailableRunB(prereq))
    }

    val panvkId = prereq.panvkDriverId!!
    val stock = prereq.stockDxvkDir!!
    val dxvm = DxvmComponent.installed(ctx)

    progress("Comparison: Run B — ${ComparisonMode.PANVK_DXVK_LABEL} (stock DXVK: ${stock.name})")
    val runB = try {
        // The driver and the DLLs are passed per run, not left in the saved selection: the
        // container writes its ICD override from the run's own driverId, and the harness stages
        // the DLLs from the component named here.
        val host = PanVkRunHost(AndroidTestFlowHost(ctx), panvkId, stock)
        runFullPass(host, progress) + "\n" + VULKAN_HALF_NOTE
    } finally {
        // Put DXVM's DLLs back in the prefix, and the stock DXVK out of it: a leftover baseline
        // prefix would silently invalidate the owner's next default run.
        dxvm?.let { (content, _) -> DxvmComponent.copyIntoPrefix(ctx, content.dir) }
        progress("Comparison: restored DXVM in the prefix")
    }
    mergeComparison(runA, runB)
}

/**
 * Run B's Vulkan section is produced by this app's process, which always loads the system ICD;
 * PanVK is only ever loaded inside Wine. Saying so keeps the two columns from being misread.
 */
private const val VULKAN_HALF_NOTE =
    "NOTE: the Vulkan section of this run describes the app process's system loader; " +
        "PanVK is only loaded inside the Wine container, so it appears in the Direct3D section."

private fun unavailableRunB(prereq: ComparisonPrereq): String = buildString {
    appendLine("Run B was not attempted; this is not a result.")
    for (reason in prereq.missing()) appendLine("NOTE: $reason")
    appendLine("Total: 0 passed, 0 failed or crashed")
}