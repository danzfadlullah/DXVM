package dev.zenithblue.panvklauncher

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * The Vulkan half's result mapping.
 *
 * Note on padding: the plan's sketch showed a fixed two-space separator, but the report format
 * is "name padded to column 15 with a minimum of one space" (plan Global Constraints) and the
 * harness implements exactly that (`Report.cpp`, `kNameColumn = 15`). Both halves share one
 * formatter so the report's two sections line up; the parser accepts both widths.
 */
class VkResultParseTest {
    @Test fun mapsReturnCodesToKinds() {
        assertEquals("PASS vk.bc" + " ".repeat(10) + "BCn sampling supported",
            VkTests.format("vk.bc", 0, "BCn sampling supported"))
        // "vk.fill-mode" is 12 characters, so column 15 leaves exactly three spaces.
        assertEquals("FAIL vk.fill-mode" + " ".repeat(3) + "polygonMode LINE rejected",
            VkTests.format("vk.fill-mode", 1, "polygonMode LINE rejected"))
        assertEquals("SKIP vk.bc" + " ".repeat(10) + "no BCn formats",
            VkTests.format("vk.bc", 2, "no BCn formats"))
    }

    @Test fun anUnexpectedReturnCodeIsASkipNotAPass() {
        assertEquals("SKIP", VkTests.kind(2))
        assertEquals("SKIP", VkTests.kind(7))
        assertEquals("SKIP", VkTests.kind(-1))
        assertEquals("PASS", VkTests.kind(0))
        assertEquals("FAIL", VkTests.kind(1))
    }

    @Test fun aVulkanLineParsesWithTheD3DParser() {
        // One report, one parser: a Vulkan section must be readable by the same code.
        val line = VkTests.format("vk.multi-viewport", 0, "maxViewports 16")
        val parsed = D3DSuite.parse(line)
        assertEquals(1, parsed.size)
        assertEquals("PASS", parsed[0].kind)
        assertEquals("vk.multi-viewport", parsed[0].name)
        assertEquals("maxViewports 16", parsed[0].detail)
    }

    @Test fun identityIsNeverReportedAsSystemWithoutANativeRead() {
        // The native library is absent in a JVM test build, so the identity must degrade to
        // Unknown rather than defaulting to the driver this app is supposed to be testing.
        assertTrue(VkTests.identity().kind != DriverIdentity.Kind.System)
        assertTrue(VkTests.driverLine().startsWith("Driver: unknown"))
    }
}