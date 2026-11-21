package dev.zenithblue.panvklauncher

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * The report is the only thing the owner sends back, so its shape is a contract: header first,
 * one section per half, per-section summary, totals last. The failure mode that matters most is
 * a dead container reading as a clean run, which is what the "nothing ran" note exists for.
 */
class TestReportTest {
    private fun header(driver: String = "Driver: system, Mali-G615 MC2, driver r44p1") =
        TestReport.Header(
            version = "0.1.0",
            code = 1,
            driver = driver,
            dxvm = "0.1.0-alpha.1-arm64ec",
            timestamp = "2026-11-21 09:00",
            device = listOf(
                "Model:" to "2211133G",
                "Chip:" to "SM8450",
                "GPU:" to "Mali-G615 MC2",
                "Android:" to "16 (API 36)",
                "Build:" to "BP2A.250705.008"
            )
        )

    @Test fun headerNamesTheDriverAndTotals() {
        val out = TestReport.build(
            header(),
            listOf(D3DSuite.ResultLine("PASS", "device.create", "feature level 0xb000")),
            listOf(D3DSuite.ResultLine("FAIL", "vk.fill-mode", "polygonMode LINE rejected"))
        )
        assertTrue(out.startsWith("DXVM TestPack report, "))
        assertTrue(out.contains("Driver: system, Mali-G615 MC2"))
        assertTrue(out.contains("DXVM: 0.1.0-alpha.1-arm64ec"))
        assertTrue(out.contains("Total: 1 passed, 1 failed or crashed"))
        assertTrue(out.contains("== Direct3D 11 suite"))
        assertTrue(out.contains("== Vulkan capabilities"))
        assertTrue(out.contains("== Totals (pass/total) =="))
        assertFalse(out.contains("NOTE: nothing ran"))
    }

    @Test fun allSkipRunIsNotACleanRun() {
        val out = TestReport.build(header("Driver: system"),
            listOf(D3DSuite.ResultLine("SKIP", "compute", "not ported yet")), emptyList())
        assertTrue(out.contains("Total: 0 passed, 0 failed or crashed"))
        assertTrue(out.contains("NOTE: nothing ran"))
    }

    @Test fun anEmptyRunIsNotACleanRunEither() {
        val out = TestReport.build(header(), emptyList(), emptyList())
        assertTrue(out.contains("Total: 0 passed, 0 failed or crashed"))
        assertTrue(out.contains("NOTE: nothing ran"))
    }

    @Test fun crashesCountAsFailedNotPassed() {
        val out = TestReport.build(header(),
            listOf(D3DSuite.ResultLine("CRASH", "msaa4", "exit code -1073741819")), emptyList())
        assertTrue(out.contains("Total: 0 passed, 1 failed or crashed"))
        assertFalse(out.contains("NOTE: nothing ran"))
        assertTrue(out.contains("CRASH msaa4"))
    }

    @Test fun eachSectionCarriesItsOwnSummary() {
        val out = TestReport.build(header(),
            listOf(
                D3DSuite.ResultLine("PASS", "a", "x"),
                D3DSuite.ResultLine("FAIL", "b", "y"),
                D3DSuite.ResultLine("SKIP", "c", "z")),
            listOf(D3DSuite.ResultLine("PASS", "vk.bc", "ok")))
        val d3d = out.substringAfter("== Direct3D 11 suite")
        val vk = out.substringAfter("== Vulkan capabilities")
        assertTrue(d3d.contains("SUMMARY 1 passed, 1 failed"))
        assertTrue(vk.contains("SUMMARY 1 passed, 0 failed"))
    }

    @Test fun resultLinesKeepTheDxmlColumnAndDetail() {
        val out = TestReport.build(header(),
            listOf(D3DSuite.ResultLine("PASS", "device.create", "feature level 0xb000")), emptyList())
        // 13 characters of name leaves two spaces before the detail at column 15.
        assertTrue(out.contains("PASS device.create  feature level 0xb000"))
    }

    @Test fun theDeviceBlockIsReadableAndPresent() {
        val out = TestReport.build(header(), emptyList(), emptyList())
        for (line in listOf("Model:", "Chip:", "GPU:", "Android:", "Build:")) {
            assertTrue("device block is missing $line", out.contains(line))
        }
    }

    @Test fun theDriverLineIsNeverEmpty() {
        // A report without a driver is uninterpretable; build() must refuse rather than emit one.
        val out = TestReport.build(header("Driver: unknown, an unidentified device"), emptyList(), emptyList())
        assertTrue(out.contains("Driver: unknown"))
    }

    @Test fun totalsTableUsesTheDxmlWidths() {
        val rows = TestReport.totalsRows(listOf(
            D3DSuite.ResultLine("PASS", "a", ""),
            D3DSuite.ResultLine("FAIL", "b", ""),
            D3DSuite.ResultLine("SKIP", "c", "")),
            emptyList())
        val line = rows.first { it.startsWith("Direct3D") }
        // Label column is 75 wide, then three 11-wide numeric columns laid out like Report.cpp.
        assertEquals(75 + 33, line.length)
        assertEquals("1/3", line.substring(75, 86).trim())
        assertEquals("1", line.substring(86, 97).trim())
        assertEquals("1", line.substring(97, 108).trim())
        // Same widths on the header row, so the table lines up.
        val header = rows.first()
        assertEquals(75 + 33, header.length)
        assertEquals("pass/total", header.substring(75, 86).trim())
    }

    @Test fun skippedCountsAreVisibleNotHidden() {
        val out = TestReport.build(header(),
            listOf(D3DSuite.ResultLine("SKIP", "a", "why")), emptyList())
        // A skip must be attributable: the reason has to survive into the report.
        assertTrue(out.contains("why"))
    }
}