package dev.zenithblue.panvklauncher

import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * One DXML-shaped report per run: header, one section per half, totals.
 *
 * The format is DXML Test Pack's *observable output* — line kinds, `== section ==` headers,
 * `SUMMARY <n> passed, <m> failed`, and the totals table's measured column widths. No DXML code
 * was read or copied; DXML publishes no licence, so only the shape of its output is mirrored.
 *
 * Two rules from the design are enforced here rather than left to the caller:
 *  - the header always names the driver, because a result without driver identity cannot be used;
 *  - a run where nothing passed and nothing failed gets `NOTE: nothing ran`, so a container that
 *    never started cannot read as a clean run.
 */
object TestReport {
    /** Label column width, measured from the real DXML report (Report.cpp's totals row). */
    private const val LABEL_WIDTH = 75
    /** Each numeric column's width in the totals table. */
    private const val COLUMN_WIDTH = 11

    data class Header(
        val version: String,
        val code: Int,
        /** The `Driver: ...` line from `reportLine(identity)`. */
        val driver: String,
        /** The DXVM component version actually in the prefix. */
        val dxvm: String,
        val timestamp: String = formatNow(),
        /** Device block lines, printed as `label value` in order. */
        val device: List<Pair<String, String>> = emptyList()
    )

    private fun formatNow(): String =
        SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.US).format(Date())

    private fun count(lines: List<D3DSuite.ResultLine>): Triple<Int, Int, Int> {
        val passed = lines.count { it.kind == "PASS" }
        val failed = lines.count { it.kind == "FAIL" || it.kind == "CRASH" }
        val skipped = lines.count { it.kind == "SKIP" }
        return Triple(passed, failed, skipped)
    }

    /**
     * Builds the report. [d3d] and [vk] are the two halves' result lines; both may be empty, and
     * an empty run is reported as such instead of as a pass.
     */
    fun build(header: Header, d3d: List<D3DSuite.ResultLine>, vk: List<D3DSuite.ResultLine>): String {
        val (dPass, dFail, _) = count(d3d)
        val (vPass, vFail, _) = count(vk)
        val passed = dPass + vPass
        val failed = dFail + vFail

        return buildString {
            appendLine("DXVM TestPack report, ${header.timestamp}")
            appendLine("TestPack: ${header.version} (code ${header.code})")
            appendLine(header.driver)
            appendLine("DXVM: ${header.dxvm}")
            appendLine("Total: $passed passed, $failed failed or crashed")
            appendLine()
            if (header.device.isNotEmpty()) {
                for ((label, value) in header.device) appendLine("$label $value")
                appendLine()
            }
            // The spec's report sample names the driver in every section header, not only in the
            // header block: a section pasted on its own must still be interpretable.
            section(this, "== Direct3D 11 suite (${driverKind(header.driver)}) ==", d3d)
            section(this, "== Vulkan capabilities (${driverKind(header.driver)}) ==", vk)
            if (passed == 0 && failed == 0) {
                appendLine("NOTE: nothing ran")
                appendLine()
            }
            appendLine("== Totals (pass/total) ==")
            for (row in totalsRows(d3d, vk)) appendLine(row)
        }
    }

    /** `system driver`, `PanVK`, ... — what produced this section, taken from the driver line. */
    fun driverKind(driverLine: String): String = when {
        driverLine.startsWith("Driver: system") -> "system driver"
        driverLine.startsWith("Driver: PanVK") -> "PanVK"
        else -> "driver not identified"
    }

    private fun section(out: StringBuilder, title: String, lines: List<D3DSuite.ResultLine>) {
        out.appendLine(title)
        for (l in lines) out.appendLine(D3DSuite.formatResult(l.kind, l.name, l.detail))
        val (passed, failed, _) = count(lines)
        out.appendLine("SUMMARY $passed passed, $failed failed")
        out.appendLine()
    }

    /**
     * The totals table: a label column [LABEL_WIDTH] wide followed by three [COLUMN_WIDTH]-wide
     * numeric columns (pass/total, failed, skipped), matching DXML's measured layout.
     */
    fun totalsRows(d3d: List<D3DSuite.ResultLine>, vk: List<D3DSuite.ResultLine>): List<String> {
        val rows = mutableListOf<String>()
        rows.add(row("", "pass/total", "failed", "skipped"))
        val halves = listOf("Direct3D 11" to d3d, "Vulkan capabilities" to vk)
        for ((label, lines) in halves) {
            val (passed, failed, skipped) = count(lines)
            rows.add(row(label, "$passed/${lines.size}", "$failed", "$skipped"))
        }
        val all = d3d + vk
        val (passed, failed, skipped) = count(all)
        rows.add(row("Total", "$passed/${all.size}", "$failed", "$skipped"))
        return rows
    }

    private fun row(label: String, vararg columns: String): String {
        val sb = StringBuilder()
        sb.append(label.padEnd(LABEL_WIDTH))
        for (c in columns) sb.append(c.padEnd(COLUMN_WIDTH))
        return sb.toString()
    }
}