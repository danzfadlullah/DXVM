package dev.zenithblue.panvklauncher

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * Parsing the harness's own report. The wire format is fixed by
 * `tests/dxvm-harness/src/Report.cpp`: `<KIND> <name padded to column 15, min one space><detail>`,
 * kinds exactly PASS/FAIL/SKIP/CRASH, and `SUMMARY <N> passed, <M> failed`.
 */
class D3DSuiteTest {
    @Test fun parsesResultLinesAndSummary() {
        val text = """
            == Direct3D 11 suite (system driver) ==
            device created, feature level 0xb000
            PASS msaa2.sampleindex  red is 9, expected 9
            FAIL bcn.bc7            block fixture not ported yet
            SKIP compute            not ported yet
            SUMMARY 1 passed, 1 failed
        """.trimIndent()
        val lines = D3DSuite.parse(text)
        assertEquals(3, lines.size)
        assertEquals("msaa2.sampleindex", lines[0].name)
        assertEquals("PASS", lines[0].kind)
        assertEquals("red is 9, expected 9", lines[0].detail)
        assertEquals("FAIL", lines[1].kind)
        assertEquals("SKIP", lines[2].kind)
    }

    @Test fun emptyReportIsAFailureNotACleanRun() {
        assertTrue(D3DSuite.parse("").isEmpty())
        assertTrue(D3DSuite.parse("== Direct3D 11 suite ==\nSUMMARY 0 passed, 0 failed\n").isEmpty())
    }

    @Test fun aLongNameGetsASingleSpaceAndStillParses() {
        // "device.create.d3d11" is 19 chars, so Report.cpp pads with exactly one space.
        val lines = D3DSuite.parse("PASS device.create.d3d11 feature level 0xb000")
        assertEquals(1, lines.size)
        assertEquals("device.create.d3d11", lines[0].name)
        assertEquals("feature level 0xb000", lines[0].detail)
    }

    @Test fun namesWithSpacesSurvive() {
        val lines = D3DSuite.parse("PASS compute queue  workgroup 64x1x1")
        assertEquals("compute queue", lines[0].name)
        assertEquals("workgroup 64x1x1", lines[0].detail)
    }

    @Test fun crashIsAResultKind() {
        val lines = D3DSuite.parse("CRASH msaa4 exit code -1073741819 (0xC0000005)")
        assertEquals(1, lines.size)
        assertEquals("CRASH", lines[0].kind)
        assertEquals("msaa4", lines[0].name)
        assertEquals("exit code -1073741819 (0xC0000005)", lines[0].detail)
    }

    @Test fun nonResultLinesAreIgnored() {
        val text = """
            DXVM Test Pack report, 2026-11-21 09:00
            Harness: 0.1.0 (code 1)
            Total: 2 passed, 0 failed or crashed
            == Direct3D 11 suite (system driver) ==
            PASS device.create  feature level 0xb000
            SUMMARY 1 passed, 0 failed
        """.trimIndent()
        val lines = D3DSuite.parse(text)
        assertEquals(1, lines.size)
        assertEquals("device.create", lines[0].name)
    }

    @Test fun summaryIsParsedSoABlanketSkipCanBeSpotted() {
        val s = D3DSuite.parseSummary("== x ==\nSKIP a  b\nSKIP c  d\nSUMMARY 0 passed, 0 failed")
        assertEquals(0, s?.passed)
        assertEquals(0, s?.failed)
        assertNull(D3DSuite.parseSummary("no summary here"))
    }

    @Test fun formattingMatchesTheHarnessPadding() {
        assertEquals("PASS vk.bc" + " ".repeat(10) + "BCn sampling supported",
            D3DSuite.formatResult("PASS", "vk.bc", "BCn sampling supported"))
        // Never zero spaces, even when the name already fills the column.
        assertEquals("PASS exactly15chars detail", D3DSuite.formatResult("PASS", "exactly15chars", "detail"))
    }

    @Test fun formatAndParseRoundTrip() {
        val line = D3DSuite.formatResult("FAIL", "bcn.bc7", "block fixture not ported yet")
        val parsed = D3DSuite.parse(line)
        assertEquals(1, parsed.size)
        assertEquals("bcn.bc7", parsed[0].name)
        assertEquals("block fixture not ported yet", parsed[0].detail)
        assertEquals("FAIL", parsed[0].kind)
    }
}