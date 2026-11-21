package dev.zenithblue.panvklauncher

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * Comparison mode is optional and never implied. Its one non-negotiable property: each half of
 * the output names the driver that produced it, because the whole point of the comparison is
 * `system + DXVM` versus `PanVK + DXVK` — and a report that cannot distinguish them has failed
 * the app's purpose.
 */
class ComparisonModeTest {
    @Test fun comparisonReportNamesBothDriverRuns() {
        val out = mergeComparison(
            "Driver: system, Mali-G615 MC2\nPASS msaa2.sampleindex  red is 9, expected 9",
            "Driver: PanVK beta.9\nFAIL msaa2.sampleindex  red is 26, expected 9")
        assertTrue(out.contains("== Run A: system driver + DXVM =="))
        assertTrue(out.contains("== Run B: PanVK + stock DXVK =="))
        assertTrue(out.contains("Driver: system, Mali-G615 MC2"))
        assertTrue(out.contains("Driver: PanVK beta.9"))
    }

    @Test fun runAComesFirstAndIsComplete() {
        val a = "Driver: system, Mali-G615 MC2\nPASS device.create  feature level 0xb000"
        val out = mergeComparison(a, "Driver: PanVK\nFAIL device.create  no device")
        assertTrue(out.indexOf("== Run A") < out.indexOf("== Run B"))
        // Nothing from Run A may be dropped in the merge.
        assertTrue(out.contains("PASS device.create"))
        assertTrue(out.contains("FAIL device.create"))
    }

    @Test fun anUnreadyComparisonSaysWhatIsMissingInsteadOfSkippingQuietly() {
        val prereq = ComparisonPrereq(panvkDriverId = null, stockDxvkDir = null)
        assertFalse(prereq.ready)
        val missing = prereq.missing()
        assertTrue(missing.any { it.contains("PanVK") })
        assertTrue(missing.any { it.contains("DXVK") })
    }

    @Test fun theBaselineLabelNamesBothTheDriverAndTheComponent() {
        assertEquals("PanVK + stock DXVK", ComparisonMode.PANVK_DXVK_LABEL)
    }

    @Test fun aHalfReadyComparisonNamesOnlyTheGapThatIsLeft() {
        val prereq = ComparisonPrereq(panvkDriverId = "panvk-beta9", stockDxvkDir = null)
        assertFalse(prereq.ready)
        val missing = prereq.missing()
        assertTrue("PanVK is installed, so it must not be reported missing",
            missing.none { it.contains("PanVK driver") })
        assertTrue(missing.any { it.contains("DXVK") })
    }

    @Test fun panvkIsRecognisedFromTheDriverNameOrId() {
        fun driver(id: String = "bundled", name: String = "Mali driver", lib: String = "libvulkan.so") =
            Driver(id = id, name = name, description = "", author = "", version = "1",
                libPath = lib, bundled = true)
        assertTrue(ComparisonMode.isPanVk(driver(id = "panvk-kbase-beta9", name = "PanVK beta.9")))
        assertTrue(ComparisonMode.isPanVk(driver(lib = "/data/panvk/libvulkan_panfrost.so")))
        assertFalse(ComparisonMode.isPanVk(driver(name = "Bundled ARM driver")))
    }
}