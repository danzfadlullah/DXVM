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
    suspend fun ensureDxvm(): Result<String>

    /** Identity of the driver that will actually answer — read from the driver, not a setting. */
    fun driverIdentity(): DriverIdentity

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
suspend fun runFullPass(host: TestFlowHost, progress: (String) -> Unit): String {
    progress("DXVM component: checking")
    val dxvm = host.ensureDxvm().getOrElse { err ->
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
    return TestReport.build(host.header(dxvm), d3d, vk)
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

    override suspend fun ensureDxvm(): Result<String> = withContext(Dispatchers.IO) {
        DxvmComponent.installedVersion(ctx)?.let { return@withContext Result.success(it) }
        val asset = bundledDxvmAsset()
            ?: return@withContext Result.failure(IllegalStateException(
                "no dxvm-*.wcp is bundled in this APK and none is installed"))
        val installed = DxvmComponent.installBundled(ctx, asset).getOrElse { return@withContext Result.failure(it) }
        // The container may already exist from an earlier run: put DXVM's DLLs in it now.
        if (ContainerManager.isSetup(ctx)) {
            DxvmComponent.installed(ctx)?.let { (content, _) ->
                DxvmComponent.copyIntoPrefix(ctx, content.dir).onFailure { return@withContext Result.failure(it) }
            }
            ContainerManager.setDxvkEnabled(ctx, true)?.let {
                return@withContext Result.failure(IllegalStateException("could not enable DXVK: $it"))
            }
        }
        Result.success(installed)
    }

    override fun driverIdentity(): DriverIdentity = VkTests.identity()

    override suspend fun runD3d(progress: (String) -> Unit): List<D3DSuite.ResultLine> {
        val api = "d3d11,d3d10,d3d9"
        progress("Direct3D suite: launching dxvm-harness inside the container")
        val report = D3DSuite.run(ctx, api).getOrElse { err ->
            return listOf(D3DSuite.ResultLine("CRASH", "harness", err.message ?: "launch failed"))
        }
        val lines = D3DSuite.parse(report).toMutableList()
        // The proof the report is about DXVM and not upstream DXVK, read from the prefix itself.
        val version = DxvmComponent.prefixVersion(ctx)
        lines.add(0, if (version != null) {
            D3DSuite.ResultLine("PASS", "dxvm.prefix", "DXVM $version in drive_c/windows/system32")
        } else {
            D3DSuite.ResultLine("FAIL", "dxvm.prefix", "no DXVM marker in the Wine prefix")
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
            device = deviceBlock(identity.name)
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