package dev.zenithblue.panvklauncher

import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * The pass's order is the part that decides whether the report can be trusted, so it is tested
 * without a device: prove DXVM → identify the driver → D3D half → Vulkan half → assemble, and
 * stop immediately when DXVM is not the implementation in play.
 *
 * The plan sketched `stubCtx(dxvmInstalled = false)`; `Context` is an Android type a JVM test
 * cannot construct, so the seam is [TestFlowHost] and the stub implements it.
 */
class TestFlowTest {

    private class StubHost(
        private val dxvm: Result<String>,
        private val d3d: List<D3DSuite.ResultLine> = listOf(
            D3DSuite.ResultLine("PASS", "device.create", "feature level 0xb000")),
        private val vk: List<D3DSuite.ResultLine> = listOf(
            D3DSuite.ResultLine("FAIL", "vk.fill-mode", "polygonMode LINE rejected"))
    ) : TestFlowHost {
        var d3dRuns = 0
        var vkRuns = 0

        override suspend fun ensureDxvm(progress: (String) -> Unit): Result<String> = dxvm
        override fun driverIdentity() =
            classify(0x13b5, "", "Mali-G615 MC2, driver r44p1")
        override suspend fun runD3d(progress: (String) -> Unit): List<D3DSuite.ResultLine> {
            d3dRuns++
            return d3d
        }
        override fun runVulkan(progress: (String) -> Unit): List<D3DSuite.ResultLine> {
            vkRuns++
            return vk
        }
        override fun header(dxvm: String) = TestReport.Header(
            version = "0.1.0", code = 1,
            driver = reportLine(driverIdentity()), dxvm = dxvm,
            timestamp = "2026-11-21 09:00")
    }

    private fun stubCtx(dxvmInstalled: Boolean) = StubHost(
        if (dxvmInstalled) Result.success("0.1.0-alpha.1-arm64ec")
        else Result.failure(IllegalStateException("no dxvm-*.wcp is bundled in this APK")))

    @Test fun stopsWhenDxvmIsNotInstalled() {
        val steps = mutableListOf<String>()
        val r = runBlocking { runFullPass(stubCtx(dxvmInstalled = false), steps::add) }
        assertTrue("progress must name the component step", steps.any { it.contains("DXVM component") })
        assertTrue(r.contains("SKIP") || r.contains("FAIL"))
        assertFalse("a run without DXVM must not report Direct3D results", r.contains("PASS device.create"))
    }

    @Test fun doesNotRunTheSuiteWhenDxvmIsMissing() {
        val host = stubCtx(dxvmInstalled = false)
        runBlocking { runFullPass(host) {} }
        assertEquals("the D3D half must not run against a different D3D implementation", 0, host.d3dRuns)
        assertEquals(0, host.vkRuns)
    }

    @Test fun runsBothHalvesInOrderWhenDxvmIsPresent() {
        val steps = mutableListOf<String>()
        val r = runBlocking { runFullPass(stubCtx(dxvmInstalled = true), steps::add) }
        val component = steps.indexOfFirst { it.startsWith("DXVM component: 0.1.0") }
        val driver = steps.indexOfFirst { it.startsWith("Driver: system") }
        val d3d = steps.indexOfFirst { it.startsWith("Direct3D suite: running") }
        val vk = steps.indexOfFirst { it.startsWith("Vulkan suite: running") }
        assertTrue("component must be proven before the driver is used", component in 0 until driver)
        assertTrue("driver must be identified before a suite runs", driver in 0 until d3d)
        assertTrue("Direct3D before Vulkan", d3d in 0 until vk)
        assertTrue(r.contains("PASS device.create"))
        assertTrue(r.contains("FAIL vk.fill-mode"))
    }

    @Test fun theReportAlwaysNamesTheDriver() {
        for (installed in listOf(true, false)) {
            val r = runBlocking { runFullPass(stubCtx(installed)) {} }
            assertTrue(r.lines().first { it.startsWith("Driver:") }.startsWith("Driver: system"))
        }
    }

    @Test fun aMissingComponentStillCountsAsAFailure() {
        val r = runBlocking { runFullPass(stubCtx(dxvmInstalled = false)) {} }
        assertTrue(r.contains("Total: 0 passed, 1 failed or crashed"))
        assertFalse("a stopped run must not read as clean", r.contains("NOTE: nothing ran"))
    }

    @Test fun progressIsReportedForEveryStep() {
        val steps = mutableListOf<String>()
        runBlocking { runFullPass(stubCtx(dxvmInstalled = true), steps::add) }
        // The screen shows these; a silent step looks like a hang on a phone.
        assertTrue(steps.any { it.startsWith("Direct3D suite") })
        assertTrue(steps.any { it.startsWith("Vulkan suite") })
        assertTrue(steps.any { it.startsWith("Report:") })
    }
}