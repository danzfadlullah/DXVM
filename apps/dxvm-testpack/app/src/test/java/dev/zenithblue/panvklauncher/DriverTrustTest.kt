package dev.zenithblue.panvklauncher

import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * The anti-goal guards for the two findings that made the first owner run untrustworthy:
 *
 *  - **C1** the Direct3D half runs inside Wine, whose ICD the container writes from the run's
 *    driver id. If that is not the system driver, the report says `system` while every D3D number
 *    came from PanVK. So the default pass must *request* the system driver, and the section
 *    headers must say which driver each half used.
 *  - **C2** the component's versionName is fixed per release, so an APK rebuilt with a newer DXVM
 *    finds "already installed" and keeps testing the old DLLs. The bundled archive's digest is
 *    what must decide.
 */
class DriverTrustTest {

    private class StubHost(
        private val dxvm: Result<String> = Result.success("0.1.0-alpha.1-arm64ec"),
        override val d3dDriverId: String = ContainerManager.SYSTEM_DRIVER_ID,
        override val d3dDriverLabel: String = "system driver + DXVM"
    ) : TestFlowHost {
        override suspend fun ensureDxvm(progress: (String) -> Unit): Result<String> = dxvm
        override fun driverIdentity() = classify(0x13b5, "", "Mali-G615 MC2, driver r44p1")
        override suspend fun runD3d(progress: (String) -> Unit) = listOf(
            D3DSuite.ResultLine("PASS", "device.create", "feature level 0xb000"))
        override fun runVulkan(progress: (String) -> Unit) = listOf(
            D3DSuite.ResultLine("PASS", "vk.bc", "BCn sampling supported"))
        override fun header(dxvm: String) = TestReport.Header(
            version = "0.1.0", code = 1,
            driver = reportLine(driverIdentity()), dxvm = dxvm,
            timestamp = "2026-11-21 09:00",
            d3dSectionLabel = d3dDriverLabel)
    }

    @Test fun theContainerWritesNoIcdOverrideForTheSystemDriver() {
        // The bug this pins: env() wrote VK_ICD_FILENAMES at the *selected* driver, which defaults
        // to the bundled PanVK, so the D3D half measured PanVK while the header said `system`.
        // Writing no override is what makes a run reach the phone's own ICD.
        assertFalse("the system driver must mean: override nothing",
            ContainerManager.icdOverrideNeeded(ContainerManager.SYSTEM_DRIVER_ID))
        assertTrue("a named driver still gets its synthesized ICD",
            ContainerManager.icdOverrideNeeded("panvk-kbase-0.1.0-rc3"))
        assertTrue("an unset driver id falls back to the saved selection, which needs an ICD",
            ContainerManager.icdOverrideNeeded(null))
        assertEquals("system", ContainerManager.SYSTEM_DRIVER_ID)
    }

    @Test fun aRebuiltComponentReplacesTheInstalledOneEvenWithTheSameVersionName() {
        // C2: the packager's versionName is fixed per release, so a rebuilt APK must be detected
        // by the bytes, not by the version string.
        assertTrue("a rebuilt APK must not keep testing the previous DXVM",
            needsDxvmReinstall(bundledDigest = "aaa", installedDigest = "bbb", installed = true))
        assertFalse("identical bytes need no reinstall",
            needsDxvmReinstall(bundledDigest = "aaa", installedDigest = "aaa", installed = true))
        assertTrue("nothing installed yet means install",
            needsDxvmReinstall(bundledDigest = null, installedDigest = null, installed = false))
        assertFalse("a sideloaded component has no bundled archive to compare against",
            needsDxvmReinstall(bundledDigest = null, installedDigest = null, installed = true))
        assertTrue("an install with no recorded digest is re-checked",
            needsDxvmReinstall(bundledDigest = "aaa", installedDigest = null, installed = true))
    }

    @Test fun eachSectionSaysWhichDriverProducedIt() {
        val out = runBlocking { runFullPass(StubHost()) {} }
        assertTrue("the Direct3D section must name its own driver",
            out.contains("== Direct3D 11 suite (system driver + DXVM) =="))
        assertTrue("the Vulkan section runs in this process and must say so",
            out.contains("== Vulkan capabilities (system driver, app process) =="))
    }

    @Test fun aPanVkRunIsLabelledPanVkAndNeverSystem() {
        val host = StubHost(
            d3dDriverId = "panvk-kbase-0.1.0-rc3",
            d3dDriverLabel = ComparisonMode.PANVK_DXVK_LABEL)
        val out = runBlocking { runFullPass(host) {} }
        assertTrue(out.contains("== Direct3D 11 suite (PanVK + stock DXVK) =="))
        assertFalse("a PanVK D3D section must not be labelled as the system driver",
            out.contains("== Direct3D 11 suite (system driver"))
        // The header still reports the process's own probe, which for this app is the system loader.
        assertTrue(out.contains("Driver: system, Mali-G615 MC2"))
    }

    @Test fun theComparisonHalfStatesThatItsVulkanNumbersAreNotPanVk() {
        val out = mergeComparison("Driver: system, Mali-G615 MC2\nPASS a  b",
            "Driver: system, Mali-G615 MC2\n== Direct3D 11 suite (PanVK + stock DXVK) ==")
        assertTrue(out.contains("== Run B: PanVK + stock DXVK =="))
    }

    @Test fun aMissingComponentStillStopsThePassBeforeAnyD3dNumber() {
        val host = StubHost(dxvm = Result.failure(IllegalStateException("not bundled")))
        val out = runBlocking { runFullPass(host) {} }
        assertFalse(out.contains("PASS device.create"))
        assertFalse(out.contains("PASS vk.bc"))
        assertTrue(out.contains("Total: 0 passed, 1 failed or crashed"))
    }

    @Test fun theMarkerReadIsSharedWithTheComponentItself() {
        // Run B stages a stock DXVK and deletes the marker beside the harness, so the proof line
        // reads the staged directory, not the prefix (which Run A filled with DXVM).
        assertEquals("container/.wine/drive_c/dxvmt", D3DSuite.STAGE_RELATIVE)
        assertTrue(D3DSuite.STAGE_RELATIVE.contains("drive_c"))
    }
}