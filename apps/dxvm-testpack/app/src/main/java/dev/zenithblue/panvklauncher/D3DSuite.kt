package dev.zenithblue.panvklauncher

import android.content.Context
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.async
import kotlinx.coroutines.cancel
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeoutOrNull
import java.io.File

/**
 * The Direct3D half: run DXVM's own harness (`dxvm-harness.exe`, built by this repository) inside
 * the Wine container and read its report.
 *
 * The harness stays the single source of scene coverage — this app only stages it and reads its
 * `--out` file, so the scene matrix keeps growing on the Windows side without touching the app.
 *
 * Placing the DXVM DLLs *next to the executable* matters: Windows searches the application
 * directory before system32, so the Direct3D implementation that runs is provably the DXVM one,
 * not whatever an earlier prefix state left in system32.
 */
object D3DSuite {
    /** One result line of either suite, in the shared DXML shape. */
    data class ResultLine(val kind: String, val name: String, val detail: String)

    /** Per-section tallies as the harness reported them. */
    data class SectionSummary(val passed: Int, val failed: Int)

    const val HARNESS_ASSET = "harness/dxvm-harness.exe"
    const val EXE_NAME = "dxvm-harness.exe"
    const val REPORT_NAME = "dxvm-report.txt"

    /** Kinds the report may contain, and the only ones this parser accepts. */
    val KINDS = listOf("PASS", "FAIL", "SKIP", "CRASH")

    /** DXML puts the detail at column 15 and never emits zero spaces (Report.cpp's kNameColumn). */
    private const val NAME_COLUMN = 15

    /** Where the harness is staged, relative to filesDir: the prefix's drive_c, i.e. `C:\` in Wine. */
    const val STAGE_RELATIVE = "container/.wine/drive_c/dxvmt"

    /** Where the harness is staged inside the prefix: `C:\dxvmt\` under Wine. */
    fun stageDir(ctx: Context): File = File(ctx.filesDir, STAGE_RELATIVE)

    /** The in-Wine path the harness writes its report to (drive_c maps to `C:`). */
    const val REPORT_WINDOWS_PATH = "C:\\dxvmt\\$REPORT_NAME"

    /**
     * The harness's command line. The API scope is passed explicitly rather than relying on the
     * harness's own default, because the report's section title claims this scope and a title
     * that does not describe what ran is exactly the kind of lie this app must not tell.
     * `--out` gives a file to read as well as the stdout the container forwards.
     */
    fun commandLine(api: String): List<String> =
        listOf("--api", api, "--out", REPORT_WINDOWS_PATH)

    /** The DXML line for one result, matching `Report.cpp`'s padding byte for byte. */
    fun formatResult(kind: String, name: String, detail: String): String {
        val pad = if (name.length >= NAME_COLUMN) 1 else NAME_COLUMN - name.length
        return kind + " " + name + " ".repeat(pad) + detail
    }

    /**
     * Extracts every result line from a report.
     *
     * A line is `<KIND> <name><padding><detail>`; the name field may itself contain spaces, so the
     * split is at the first run of two or more spaces and falls back to the first single space for
     * a name that already fills the column. Headers, the device line and `SUMMARY` are not results
     * and are skipped — which is why an empty report yields an empty list rather than a pass.
     */
    fun parse(report: String): List<ResultLine> {
        val out = mutableListOf<ResultLine>()
        for (raw in report.lineSequence()) {
            val line = raw.trimEnd()
            if (line.isEmpty()) continue
            val kind = KINDS.firstOrNull { line.startsWith("$it ") } ?: continue
            val rest = line.substring(kind.length + 1)
            if (rest.isBlank()) continue
            val split = firstDoubleSpace(rest)
            val name: String
            val detail: String
            if (split > 0) {
                name = rest.substring(0, split)
                detail = rest.substring(split).trimStart()
            } else {
                val single = rest.indexOf(' ')
                if (single < 0) {
                    name = rest
                    detail = ""
                } else {
                    name = rest.substring(0, single)
                    detail = rest.substring(single + 1)
                }
            }
            out.add(ResultLine(kind, name.trim(), detail))
        }
        return out
    }

    private fun firstDoubleSpace(s: String): Int {
        var i = 1
        while (i < s.length) {
            if (s[i] == ' ' && s[i - 1] == ' ') return i - 1
            i++
        }
        return -1
    }

    /** `SUMMARY <N> passed, <M> failed`, or null when the report has none. */
    fun parseSummary(report: String): SectionSummary? {
        val re = Regex("""SUMMARY\s+(\d+)\s+passed,\s+(\d+)\s+failed""")
        val m = re.find(report) ?: return null
        return SectionSummary(m.groupValues[1].toInt(), m.groupValues[2].toInt())
    }

    /**
     * Stages and runs the harness, returning the report text.
     *
     * Never returns an empty success: when the container is not set up, the harness cannot be
     * staged, it times out, or it exits without writing a report, the result is a report
     * containing a single `CRASH` line naming what happened, so a dead container cannot read as
     * `0 failed`.
     */
    suspend fun run(
        ctx: Context,
        api: String = "d3d11,d3d10,d3d9",
        timeoutMs: Long = 20L * 60_000L
    ): Result<String> = withContext(Dispatchers.IO) {
        val section = "== Direct3D 11 suite ($api) =="
        if (!ContainerManager.isSetup(ctx)) {
            return@withContext Result.success(crashReport(section, api,
                "Wine container is not set up; nothing ran"))
        }
        val dxvm = DxvmComponent.installed(ctx)
            ?: return@withContext Result.success(crashReport(section, api,
                "DXVM component is not installed; refusing to test a different D3D implementation"))

        val stage = stageDir(ctx).apply { mkdirs() }
        val exe = File(stage, EXE_NAME)
        val report = File(stage, REPORT_NAME)
        try {
            report.delete()
            ctx.assets.open(HARNESS_ASSET).use { ins ->
                exe.outputStream().use { outs -> ins.copyTo(outs) }
            }
            // Next to the exe so the application-directory search finds DXVM, not system32 leftovers.
            for (dll in DxvmComponent.DLLS) {
                val src = File(File(dxvm.first.dir, "system32"), dll)
                if (src.isFile) src.copyTo(File(stage, dll), overwrite = true)
            }
        } catch (t: Throwable) {
            return@withContext Result.success(crashReport(section, api,
                "could not stage $EXE_NAME: ${t.message}"))
        }

        val captured = java.util.Collections.synchronizedList(mutableListOf<String>())
        // ContainerManager.runExe is blocking and cannot be interrupted, so it runs on its own
        // scope: on timeout the caller returns a CRASH report and asks the container to stop,
        // rather than hanging the test screen forever.
        val worker = CoroutineScope(Dispatchers.IO + SupervisorJob())
        val job = worker.async {
            ContainerManager.runExe(ctx, exe.absolutePath, { line -> captured.add(line) },
                LaunchOptions(args = commandLine(api)))
        }
        val exit = withTimeoutOrNull(timeoutMs) { job.await() }
        if (exit == null) {
            job.cancel()
            worker.cancel()
            ContainerManager.stop(ctx)
            return@withContext Result.success(crashReport(section, api,
                "harness did not finish within ${timeoutMs / 1000} s"))
        }
        worker.cancel()

        val fromFile = if (report.isFile) report.readText() else ""
        val text = if (fromFile.isNotBlank()) fromFile else captured.joinToString("\n")
        if (parse(text).isEmpty()) {
            return@withContext Result.success(crashReport(section, api,
                "harness wrote no result; exit code $exit"))
        }
        Result.success(text)
    }

    /** A one-line-failure report, so a missing run is visible instead of silently clean. */
    private fun crashReport(section: String, api: String, why: String): String = buildString {
        appendLine(section)
        appendLine(formatResult("CRASH", "harness", why))
        appendLine("SUMMARY 0 passed, 1 failed")
    }

}