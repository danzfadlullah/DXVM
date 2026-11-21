package dev.zenithblue.panvklauncher

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * The report's headline: which driver produced these numbers. The app's default and only
 * headline configuration is the phone's own driver, so anything not positively identified as
 * the system driver must never be labelled `system`.
 */
class DriverIdentityTest {
    @Test fun armProprietaryIsSystem() {
        val id = classify(0x13b5, "0x13b5", "Mali-G615 MC2, driver r44p1")
        assertEquals(DriverIdentity.Kind.System, id.kind)
        assertEquals("Driver: system, Mali-G615 MC2, driver r44p1", reportLine(id))
    }

    @Test fun mesaPanvkIsPanVk() {
        val id = classify(0x10005, "VK_DRIVER_ID_MESA_PANVK", "panvk")
        assertEquals(DriverIdentity.Kind.PanVk, id.kind)
        assertTrue(reportLine(id).startsWith("Driver: PanVK"))
    }

    @Test fun unknownIsNeverSystem() {
        assertEquals(DriverIdentity.Kind.Unknown, classify(0, "?", "?").kind)
        assertTrue(reportLine(classify(0, "?", "?")).startsWith("Driver: unknown"))
    }

    @Test fun panvkNameAloneIsEnough() {
        // A PanVK build can report a Mesa vendor id and no driverID string at all.
        assertEquals(DriverIdentity.Kind.PanVk, classify(0x10005, "", "PanVK beta.9").kind)
    }

    @Test fun armVendorLosesToExplicitPanvk() {
        // PanVK imported on a Mali phone still reports a Mesa vendor id, but if a build ever
        // reports ARM's id together with PanVK's name, the name wins: never claim `system`
        // for a driver that is not the phone's own.
        assertEquals(DriverIdentity.Kind.PanVk, classify(0x13b5, "VK_DRIVER_ID_MESA_PANVK", "panvk").kind)
    }

    @Test fun otherVendorsAreUnknownNotSystem() {
        assertEquals(DriverIdentity.Kind.Unknown, classify(0x5143, "VK_DRIVER_ID_QUALCOMM", "Adreno").kind)
        assertEquals(DriverIdentity.Kind.Unknown, classify(0x8086, "VK_DRIVER_ID_INTEL", "Intel").kind)
    }

    @Test fun melonAndLavapipeAreNotSystem() {
        // A host ICD leak (lavapipe/llvmpipe) must never read as the phone's driver.
        assertEquals(DriverIdentity.Kind.Unknown, classify(0x10005, "VK_DRIVER_ID_MESA_LLVMPIPE", "llvmpipe").kind)
    }

    @Test fun reportLineEverNamesTheDriver() {
        for (id in listOf(
            classify(0x13b5, "", "Mali-G615"),
            classify(0x10005, "VK_DRIVER_ID_MESA_PANVK", "panvk"),
            classify(0, "", ""))) {
            assertTrue("a report line without a driver name is uninterpretable",
                reportLine(id).startsWith("Driver: ") && reportLine(id).length > "Driver: ".length)
        }
    }

    @Test fun mesaDriverVersionIsDecoded() {
        // Mesa packs driverVersion as major<<22 | minor<<12 | patch.
        assertEquals("26.3.0", decodeMesaDriverVersion((26L shl 22) or (3L shl 12) or 0L))
        assertEquals("", decodeMesaDriverVersion(0))
    }

    @Test fun undefinedDriverIdIsNormalised() {
        // vkGetPhysicalDeviceProperties reports 0 when the driverID string is unavailable.
        val id = classify(0x13b5, "0", "Mali-G615 MC2")
        assertEquals(DriverIdentity.Kind.System, id.kind)
        assertEquals("Mali-G615 MC2", id.name)
    }
}