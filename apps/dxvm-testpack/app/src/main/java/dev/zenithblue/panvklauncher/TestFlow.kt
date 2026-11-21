package dev.zenithblue.panvklauncher

import android.content.Context
import android.os.Build
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.File

/**
 * What a full test pass needs from its environment.
 *
 * This exists so the pass's *order* — the part that decides whether the report is truthful — can
 * be tested on the JVM. `Context` is an Android type that cannot be constructed in a unit test,
 * so the real implementation ([AndroidTestFlowHost]) wraps it and the test supplies a stub.
 */
interface TestFlowHost {
    /**
     * Proves DXVM is the D3D implementation in play, installing the bundled component first if
     * it is not installed yet. Returns the component version, or a failure whose message is
     * shown in the report.
     */
    suspend fun ensureDxvm(progress: (String) -> Unit): Result<String>

    /** Identity of the driver that will actually answer — read from the driver, not a setting. */
    fun driverIdentity(): DriverIdentity

    /**
     * Which ICD the Direct3D half asks the container to use. [ContainerManager.SYSTEM_DRIVER_ID]
     * (the default) means the phone's own driver, i.e. no `VK_ICD_FILENAMES` override at all.
     */
    val d3dDriverId: String get() = ContainerManager.SYSTEM_DRIVER_ID

    /** Human label for the D3D half's driver, used in the report's section header. */
    val d3dDriverLabel: String get() = "system driver + DXVM"

    /** Runs the Direct3D half. Must never throw: a dead container is a CRASH line, not an exception. */
    suspend fun runD3d(progress: (String) -> Unit): List<D3DSuite.ResultLine>

    /** Runs the Vulkan half. */
    fun runVulkan(progress: (String) -> Unit): List<D3DSuite.ResultLine>

    /** Report header once the DXVM version and the driver identity are known. */
    fun header(dxvm: String): TestReport.Header
}

/**
 * One full pass, in the only order that can produce a trustworthy report:
 *
 *   prove DXVM → identify the driver → Direct3D half → Vulkan half → assemble
 *
 * A missing DXVM component **short-circuits**: testing upstream DXVK while the report says DXVM
 * is this app's headline anti-goal, so the pass stops and says why instead of producing numbers
 * about the wrong implementation.
 */
/**
 * Whether the installed DXVM component must be replaced by the one bundled in this APK.
 *
 * The component's `versionName` is fixed per release and PanPlay's installer refuses an existing
 * one, so a rebuilt APK would otherwise find "already installed" and keep testing the previous
 * DLLs while printing the same version — the owner would conclude a DXVM fix did nothing. The
 * bundled archive's digest is therefore the identity that decides.
 *
 * Pure so the rule is testable without a device.
 */
fun needsDxvmReinstall(
    bundledDigest: String?,
    installedDigest: String?,
    installed: Boolean
): Boolean {
    if (!installed) return true
    // No bundled archive (a sideloaded .wcp) or no recorded digest: nothing to compare against,
    // so take what is installed at face value rather than reinstalling on every pass.
    if (bundledDigest.isNullOrEmpty()) return false
    return bundledDigest != installedDigest
}

suspend fun runFullPass(host: TestFlowHost, progress: (String) -> Unit): String {
    progress("DXVM component: checking")
    val dxvm = host.ensureDxvm(progress).getOrElse { err ->
        val why = err.message ?: "DXVM component is not available"
        progress("DXVM component: $why")
        return shortCircuit(host, "dxvm.component", why)
    }
    progress("DXVM component: $dxvm")

    progress("Driver: identifying")
    val identity = host.driverIdentity()
    progress(reportLine(identity))
    if (identity.kind == DriverIdentity.Kind.Unknown) {
        // Not fatal — Vulkan still answers — but the owner has to know the run is not the
        // system-driver measurement the app exists to produce.
        progress("Warning: driver not identified as the system driver; the report says so")
    }

    progress("Direct3D suite: running")
    val d3d = host.runD3d(progress)

    progress("Vulkan suite: running")
    val vk = host.runVulkan(progress)

    val lines = d3d + vk
    progress("Report: ${lines.count { it.kind == "PASS" }} passed, " +
        "${lines.count { it.kind == "FAIL" || it.kind == "CRASH" }} failed or crashed")
    val header = host.header(dxvm).copy(
        d3dSectionLabel = host.d3dDriverLabel,
        vkSectionLabel = vkSectionLabel(host)
    )
    return TestReport.build(header, d3d, vk)
}

/** The Vulkan half always runs in this app's process, on whatever ICD the system loader picks. */
private fun vkSectionLabel(host: TestFlowHost): String {
    val identity = try {
        host.driverIdentity()
    } catch (_: Throwable) {
        return "driver not identified (app process)"
    }
    return when (identity.kind) {
        DriverIdentity.Kind.System -> "system driver, app process"
        DriverIdentity.Kind.PanVk -> "PanVK, app process"
        DriverIdentity.Kind.Unknown -> "driver not identified, app process"
    }
}

/** The pass over a real device. */
suspend fun runFullPass(ctx: Context, progress: (String) -> Unit): String =
    runFullPass(AndroidTestFlowHost(ctx), progress)

/**
 * A report for a pass that stopped before it could measure anything. It still names the driver
 * and still counts one failure, so a stopped run cannot be mistaken for a clean one.
 */
private fun shortCircuit(host: TestFlowHost, name: String, why: String): String {
    val header = try {
        host.header("not installed")
    } catch (_: Throwable) {
        TestReport.Header(
            version = "?", code = 0,
            driver = "Driver: unknown, could not be read", dxvm = "not installed")
    }
    return TestReport.build(header, listOf(D3DSuite.ResultLine("FAIL", name, why)), emptyList())
}

/** The real environment: the phone's own driver, the app's bundled DXVM, the Wine container. */
class AndroidTestFlowHost(private val ctx: Context) : TestFlowHost {

    private companion object {
        /** Digest of the bundled component the last install came from. */
        const val KEY_DXVM_DIGEST = "bundled_dxvm_digest"
    }

    override suspend fun ensureDxvm(progress: (String) -> Unit): Result<String> = withContext(Dispatchers.IO) {
        val asset = bundledDxvmAsset()
        val bundledDigest = asset?.let { digestOfAsset(it) }
        val installed = DxvmComponent.installed(ctx)
        val prefs = ctx.getSharedPreferences("dxvm-testpack", Context.MODE_PRIVATE)
        val installedDigest = prefs.getString(KEY_DXVM_DIGEST, null)

        // The component's versionName is fixed per release, so an APK rebuilt with a newer DXVM
        // would otherwise find "already installed" and keep testing the previous DLLs — the
        // report would print the same version and the owner would conclude a fix did nothing.
        // The bundled archive's digest is what decides; an install without a bundled archive
        // (a sideloaded .wcp) is taken at face value.
        if (!needsDxvmReinstall(bundledDigest, installedDigest, installed != null)) {
            return@withContext Result.success(installed?.second ?: "unknown")
        }

        if (asset == null) {
            return@withContext if (installed != null) {
                Result.success(installed.second)
            } else {
                Result.failure(IllegalStateException(
                    "no dxvm-*.wcp is bundled in this APK and none is installed"))
            }
        }

        if (installed != null) {
            progress("DXVM component: bundled archive differs from the installed one; reinstalling")
            if (!ContentManager.delete(ctx, installed.first)) {
                return@withContext Result.failure(IllegalStateException(
                    "could not replace the installed DXVM component ${installed.second}"))
            }
        }
        val version = DxvmComponent.installBundled(ctx, asset).getOrElse { return@withContext Result.failure(it) }
        prefs.edit().putString(KEY_DXVM_DIGEST, bundledDigest).apply()
        // The container may already exist from an earlier run: put DXVM's DLLs in it now.
        if (ContainerManager.isSetup(ctx)) {
            DxvmComponent.installed(ctx)?.let { (content, _) ->
                DxvmComponent.copyIntoPrefix(ctx, content.dir).onFailure { return@withContext Result.failure(it) }
            }
            ContainerManager.setDxvkEnabled(ctx, true)?.let {
                return@withContext Result.failure(IllegalStateException("could not enable DXVK: $it"))
            }
        }
        Result.success(version)
    }

    /** SHA-256 of the APK-bundled component archive, the identity that survives a version bump miss. */
    private fun digestOfAsset(asset: String): String = try {
        val md = java.security.MessageDigest.getInstance("SHA-256")
        ctx.assets.open(asset).use { ins ->
            val buf = ByteArray(1 shl 16)
            while (true) {
                val n = ins.read(buf)
                if (n < 0) break
                md.update(buf, 0, n)
            }
        }
        md.digest().joinToString("") { "%02x".format(it) }
    } catch (_: Exception) {
        ""
    }

    override fun driverIdentity(): DriverIdentity = VkTests.identity()

    override suspend fun runD3d(progress: (String) -> Unit): List<D3DSuite.ResultLine> =
        runD3d(progress, driverId = d3dDriverId, stageFrom = null)

    suspend fun runD3d(
        progress: (String) -> Unit,
        driverId: String,
        stageFrom: File?
    ): List<D3DSuite.ResultLine> {
        val api = "d3d11,d3d10,d3d9"
        progress("Direct3D suite: launching dxvm-harness inside the container" +
            if (driverId == ContainerManager.SYSTEM_DRIVER_ID) " on the system driver"
            else " on driver '$driverId'")
        val report = D3DSuite.run(ctx, api, driverId = driverId, stageFrom = stageFrom).getOrElse { err ->
            return listOf(D3DSuite.ResultLine("CRASH", "harness", err.message ?: "launch failed"))
        }
        val lines = D3DSuite.parse(report).toMutableList()
        // The proof of which D3D implementation ran, read from the staged files next to the
        // harness — the directory Windows actually searches first. Run B stages a stock DXVK, so
        // there the absence of DXVM's marker is the expected, correct answer.
        val staged = DxvmComponent.prefixMarkerVersion(D3DSuite.stageDir(ctx))
        val useDxvm = stageFrom == null
        lines.add(0, when {
            useDxvm && staged != null ->
                D3DSuite.ResultLine("PASS", "dxvm.staged", "DXVM $staged staged beside the harness")
            useDxvm ->
                D3DSuite.ResultLine("FAIL", "dxvm.staged", "no DXVM marker beside the harness")
            else ->
                D3DSuite.ResultLine("SKIP", "dxvm.staged",
                    "comparison run: stock DXVK staged, not DXVM")
        })
        return lines
    }

    override fun runVulkan(progress: (String) -> Unit): List<D3DSuite.ResultLine> {
        progress("Vulkan suite: asking the driver ${VkTests.names().size} questions")
        return VkTests.runAll()
    }

    override fun header(dxvm: String): TestReport.Header {
        val identity = runCatching { driverIdentity() }
            .getOrElse { DriverIdentity(DriverIdentity.Kind.Unknown, "could not be read") }
        val driver = reportLine(identity)
        return TestReport.Header(
            version = appVersionName(),
            code = appVersionCode(),
            driver = driver,
            dxvm = dxvm,
            device = deviceBlock(identity.name),
            d3dSectionLabel = d3dDriverLabel,
            vkSectionLabel = vkSectionLabel(this)
        )
    }

    private fun appVersionName(): String = try {
        ctx.packageManager.getPackageInfo(ctx.packageName, 0).versionName ?: "?"
    } catch (_: Exception) {
        "?"
    }

    private fun appVersionCode(): Int = try {
        val info = ctx.packageManager.getPackageInfo(ctx.packageName, 0)
        if (Build.VERSION.SDK_INT >= 28) (info.longVersionCode and 0xFFFFFFFFL).toInt() else @Suppress("DEPRECATION") info.versionCode
    } catch (_: Exception) {
        0
    }

    private fun deviceBlock(gpu: String): List<Pair<String, String>> = listOf(
        "Model:" to "${Build.MANUFACTURER} ${Build.MODEL}".trim(),
        "Chip:" to runCatching {
            if (Build.VERSION.SDK_INT >= 31) Build.SOC_MODEL else Build.HARDWARE
        }.getOrDefault(Build.HARDWARE).orEmpty(),
        "GPU:" to gpu,
        "Android:" to "${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})",
        "Build:" to Build.ID
    )

    /** The DXVM component archive the APK carries, if any (`components/dxvm-<version>-arm64ec.wcp`). */
    private fun bundledDxvmAsset(): String? = try {
        ctx.assets.list("components")
            ?.firstOrNull { it.startsWith("dxvm-") && it.endsWith(".wcp") }
            ?.let { "components/$it" }
    } catch (_: Exception) {
        null
    }

    /** Where the last report was written so the share sheet has a file to hand out. */
    fun reportFile(): File = File(ctx.filesDir, "dxvm-testpack-report.txt")
}