package dev.zenithblue.panvklauncher

/**
 * The Vulkan half: what the driver under test can actually do.
 *
 * The suites themselves are C (`app/src/main/cpp/vktests/vktests.c`, verified on the build
 * host against lavapipe) and answer one question each with return code 0 pass / 1 fail /
 * 2 skip. This object turns those answers into DXML lines using the *same* formatter as the
 * Direct3D half, so the two sections of the report line up, and turns the device identity
 * into the header's driver line.
 *
 * The suite points at whatever ICD the process loads — the phone's own driver unless the owner
 * explicitly selected PanVK for a comparison run. It never sets `VK_ICD_FILENAMES` itself:
 * silently swapping the driver is the failure mode this app exists to avoid.
 */
object VkTests {
    /** Kind for a suite's return code. Anything unexpected is a SKIP, never a PASS. */
    fun kind(code: Int): String = when (code) {
        0 -> "PASS"
        1 -> "FAIL"
        else -> "SKIP"
    }

    /** One DXML result line. Shares D3DSuite's padding so both sections are one table. */
    fun format(name: String, code: Int, detail: String): String =
        D3DSuite.formatResult(kind(code), name, detail)

    private val library: Boolean by lazy {
        try {
            System.loadLibrary("vktests"); true
        } catch (_: Throwable) {
            false
        }
    }

    /** True when the native suite is present in this build; the screen says so when it is not. */
    fun available(): Boolean = library

    private external fun runNative(name: String): String
    private external fun identityNative(): String
    private external fun namesNative(): Array<String>

    /** Names of the suites the native side registered, in run order. */
    fun names(): List<String> {
        if (!library) return emptyList()
        return try {
            namesNative().toList()
        } catch (_: Throwable) {
            emptyList()
        }
    }

    /**
     * Runs one suite. A native failure is reported as a SKIP *with the reason*, never dropped:
     * a missing line would be a bug, an explicit skip is acceptable.
     */
    fun run(name: String): ResultLine {
        if (!library) {
            return ResultLine("SKIP", name, "Vulkan suite not present in this build")
        }
        return try {
            val raw = runNative(name)
            val bar = raw.indexOf('|')
            if (bar < 0) return ResultLine("FAIL", name, "malformed suite result: $raw")
            val code = raw.substring(0, bar).trim().toIntOrNull() ?: 2
            ResultLine(kind(code), name, raw.substring(bar + 1))
        } catch (t: Throwable) {
            ResultLine("SKIP", name, "suite call failed: ${t.message}")
        }
    }

    /** Runs every registered suite. */
    fun runAll(): List<ResultLine> = names().map { run(it) }

    /**
     * The device and driver that answered: vendor id, driver id, device name, driver name,
     * driver info and the raw `driverVersion`.
     */
    fun identity(): DriverIdentity {
        if (!library) {
            return DriverIdentity(DriverIdentity.Kind.Unknown,
                "Vulkan suite not present in this build")
        }
        val raw = try {
            identityNative()
        } catch (t: Throwable) {
            "0|0||Vulkan identity read failed: ${t.message}|||0"
        }
        val f = raw.split('|')
        fun at(i: Int) = f.getOrNull(i)?.trim().orEmpty()
        val vendorId = at(0).toLongOrNull()?.toInt() ?: 0
        val driverVersion = at(6).toLongOrNull() ?: 0L
        val deviceName = at(3)
        val driverInfo = at(5)
        if (vendorId == 0 && deviceName.isEmpty()) {
            return DriverIdentity(DriverIdentity.Kind.Unknown,
                "no Vulkan device could be read")
        }
        // The system driver reports its device name plus a version string ("r44p1"), which is
        // how the owner recognises the phone's own blob in the report.
        val display = buildString {
            append(deviceName.ifEmpty { "unidentified device" })
            if (driverInfo.isNotEmpty()) append(", driver ").append(driverInfo)
        }
        val base = classify(vendorId, at(2), display)
        return if (base.kind == DriverIdentity.Kind.PanVk) {
            base.copy(version = decodeMesaDriverVersion(driverVersion))
        } else {
            base
        }
    }

    /** The report's driver line. */
    fun driverLine(): String = reportLine(identity())
}