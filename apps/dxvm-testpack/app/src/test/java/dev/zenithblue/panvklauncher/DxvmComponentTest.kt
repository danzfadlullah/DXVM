package dev.zenithblue.panvklauncher

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File

/**
 * The anti-goal guard for Task 4: a silent fallback to PanPlay's upstream DXVK must be
 * impossible to mistake for DXVM. Everything here is pure file/JSON logic so it runs on
 * the JVM without a device.
 */
class DxvmComponentTest {
    @get:Rule val tmp = TemporaryFolder()

    @Test fun rejectsNonDxvkType() {
        val r = DxvmComponent.validateProfile("""{"type":"VKD3D","versionName":"x"}""")
        assertTrue("a VKD3D component must not be installed as the DXVK slot", r.isFailure)
    }

    @Test fun acceptsDxvkType() {
        val r = DxvmComponent.validateProfile("""{"type":"DXVK","versionName":"0.1.0"}""")
        assertTrue(r.exceptionOrNull()?.message.orEmpty(), r.isSuccess)
        assertEquals("0.1.0", r.getOrNull()?.versionName)
    }

    @Test fun rejectsEmptyVersionName() {
        assertTrue(DxvmComponent.validateProfile("""{"type":"DXVK","versionName":""}""").isFailure)
        assertTrue(DxvmComponent.validateProfile("""{"type":"DXVK"}""").isFailure)
        assertTrue(DxvmComponent.validateProfile("""{"type":"DXVK","versionName":"."}""").isFailure)
    }

    @Test fun rejectsMalformedJson() {
        assertTrue(DxvmComponent.validateProfile("not json at all").isFailure)
        assertTrue(DxvmComponent.validateProfile("").isFailure)
    }

    @Test fun detectsDxvmByMarker() {
        val dir = tmp.newFolder("windows")
        File(dir, "system32").mkdirs()
        File(dir, "system32/d3d11.dll").writeBytes(ByteArray(4))
        assertFalse("d3d11.dll alone is what an upstream DXVK component also installs",
            DxvmComponent.provesDxvmInPrefix(dir))
        File(dir, "system32/dxvm-marker.txt").writeText("DXVM")
        assertTrue(DxvmComponent.provesDxvmInPrefix(dir))
    }

    @Test fun markerWithoutD3d11IsNotProof() {
        val dir = tmp.newFolder("windows2")
        File(dir, "system32").mkdirs()
        File(dir, "system32/dxvm-marker.txt").writeText("0.1.0-alpha.1-arm64ec")
        assertFalse("a marker with no d3d11.dll means the DLLs were never installed",
            DxvmComponent.provesDxvmInPrefix(dir))
    }

    @Test fun missingPrefixIsNotProof() {
        assertFalse(DxvmComponent.provesDxvmInPrefix(File(tmp.root, "does-not-exist")))
    }

    @Test fun markerVersionIsReadBack() {
        val dir = tmp.newFolder("windows3")
        File(dir, "system32").mkdirs()
        File(dir, "system32/d3d11.dll").writeBytes(ByteArray(4))
        File(dir, "system32/dxvm-marker.txt").writeText("0.1.0-alpha.1-arm64ec\n")
        assertEquals("0.1.0-alpha.1-arm64ec", DxvmComponent.prefixMarkerVersion(dir))
        assertNull(DxvmComponent.prefixMarkerVersion(File(tmp.root, "nope")))
    }

    @Test fun installedComponentIsOnlyDxvmWithTheMarker() {
        // A stock DXVK install (profile type DXVK, no marker) must not be reported as DXVM.
        val stock = tmp.newFolder("contents/DXVK/3.1.1-4-arm64ec")
        File(stock, "system32").mkdirs()
        File(stock, "system32/d3d11.dll").writeBytes(ByteArray(4))
        File(stock, "profile.json").writeText("""{"type":"DXVK","versionName":"3.1.1-4-arm64ec"}""")
        assertNull(DxvmComponent.versionOf(stock))

        File(stock, "system32/dxvm-marker.txt").writeText("0.1.0-alpha.1-arm64ec\n")
        assertEquals("0.1.0-alpha.1-arm64ec", DxvmComponent.versionOf(stock))
    }
}