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
    /** The installed PanVK driver's id, or null when no PanVK driver is installed. */
    val panvkDriverId: String?,
    /** A DXVK component that is *not* DXVM (no marker), or null when none is installed. */
    val stockDxvkDir: File?
) {
    val ready: Boolean get() = panvkDriverId != null && stockDxvkDir != null

    /** Exactly what is missing, for Run B's section — a silent skip would look like a done job. */
    fun missing(): List<String> = buildList {
        if (panvkDriverId == null) add("no PanVK driver is installed (Drivers screen)")
        if (stockDxvkDir == null) add("no stock DXVK component is installed (Components screen)")
    }
}

object ComparisonMode {
    /** Reads what the comparison would use, without changing anything. */
    fun prereq(ctx: Context): ComparisonPrereq {
        val panvk = DriverManager.getDrivers(ctx).firstOrNull { isPanVk(it) }?.id
        val stock = ContentManager.list(ctx)
            .firstOrNull { it.type == "DXVK" && DxvmComponent.versionOf(it.dir) == null }
            ?.dir
        return ComparisonPrereq(panvk, stock)
    }

    fun isPanVk(driver: Driver): Boolean =
        driver.name.contains("panvk", ignoreCase = true) ||
            driver.id.contains("panvk", ignoreCase = true) ||
            driver.libPath.contains("panvk", ignoreCase = true)
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
    val previousDriver = DriverManager.getSelectedDriverId(ctx)
    val dxvm = DxvmComponent.installed(ctx)

    progress("Comparison: Run B — PanVK + stock DXVK")
    val runB = try {
        DriverManager.setSelectedDriverId(ctx, panvkId)
        // Put the stock DXVK in the prefix for this run; DXVM's copy is restored afterwards.
        DxvmComponent.copyAnyIntoPrefix(ctx, stock).getOrElse {
            return@withContext mergeComparison(runA, "Run B could not start: ${it.message}")
        }
        runFullPass(ctx, progress)
    } finally {
        // Always put the default configuration back: a leftover PanVK prefix would silently
        // invalidate the owner's next system-driver run.
        if (previousDriver != null) DriverManager.setSelectedDriverId(ctx, previousDriver)
        dxvm?.let { (content, _) -> DxvmComponent.copyAnyIntoPrefix(ctx, content.dir) }
        progress("Comparison: restored ${previousDriver ?: "the default driver"} with DXVM")
    }
    mergeComparison(runA, runB)
}

private fun unavailableRunB(prereq: ComparisonPrereq): String = buildString {
    appendLine("Run B was not attempted; this is not a result.")
    for (reason in prereq.missing()) appendLine("NOTE: $reason")
    appendLine("Total: 0 passed, 0 failed or crashed")
}