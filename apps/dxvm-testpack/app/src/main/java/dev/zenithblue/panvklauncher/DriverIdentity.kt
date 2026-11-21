package dev.zenithblue.panvklauncher

/**
 * Which Vulkan driver produced a run's numbers.
 *
 * The whole point of this app is the phone's **own** driver, so `system` is a claim that must be
 * earned: only ARM's vendor id (Mali) qualifies, and an explicit PanVK name always wins over a
 * vendor id, because PanVK reports Mesa's. Everything else — including a host ICD such as
 * lavapipe leaking in — is `Unknown`, never `system`. A report that cannot tell the two drivers
 * apart has failed the app's purpose, so an unidentified driver is reported as unidentified
 * rather than guessed.
 */
data class DriverIdentity(
    val kind: Kind,
    /** Device/driver name as the driver reports it, e.g. "Mali-G615 MC2". */
    val name: String,
    /** Vendor-specific driverVersion string when it could be decoded, else "". */
    val version: String = ""
) {
    enum class Kind { System, PanVk, Unknown }
}

/** ARM Ltd. — the Mali blob is the only vendor whose driver is "the system driver" here. */
const val VENDOR_ARM = 0x13B5

/** Mesa's vendor id, used by both PanVK and lavapipe. */
const val VENDOR_MESA = 0x10005

private const val MESA_PANVK_DRIVER_ID = "VK_DRIVER_ID_MESA_PANVK"

/** `VK_DRIVER_ID_MESA_PANVK`'s numeric value, for a driver that only reports the enum. */
private const val MESA_PANVK_DRIVER_ID_VALUE = 30

/**
 * Classifies a physical device from the values Vulkan reports for it.
 *
 * @param vendorId `VkPhysicalDeviceProperties.vendorID`
 * @param driverId the `VkPhysicalDeviceDriverProperties.driverID` spelling (`VK_DRIVER_ID_...`)
 *   or the raw numeric value as a string; "0" and "" both mean "not reported"
 * @param driverName `VkPhysicalDeviceDriverProperties.driverName`, or the device name when the
 *   driver-properties extension is unavailable
 */
fun classify(vendorId: Int, driverId: String, driverName: String): DriverIdentity {
    val id = driverId.trim()
    val name = driverName.trim()
    val looksPanvk = id.contains(MESA_PANVK_DRIVER_ID, ignoreCase = true) ||
        id == MESA_PANVK_DRIVER_ID_VALUE.toString() ||
        name.contains("panvk", ignoreCase = true)

    if (looksPanvk) {
        return DriverIdentity(DriverIdentity.Kind.PanVk, name.ifEmpty { "panvk" })
    }
    if (vendorId == VENDOR_ARM) {
        return DriverIdentity(DriverIdentity.Kind.System, name.ifEmpty { "Mali" }, "")
    }
    return DriverIdentity(DriverIdentity.Kind.Unknown, name.ifEmpty { "an unidentified device" }, "")
}

/**
 * The one line the report header uses. It always names a driver: a result whose driver is
 * missing is uninterpretable for this project.
 */
fun reportLine(identity: DriverIdentity): String = when (identity.kind) {
    DriverIdentity.Kind.System -> "Driver: system, ${identity.name}"
    DriverIdentity.Kind.PanVk ->
        "Driver: PanVK" + if (identity.version.isNotBlank()) " ${identity.version}" else ""
    DriverIdentity.Kind.Unknown -> "Driver: unknown, ${identity.name}"
}

/**
 * Mesa packs `driverVersion` as major<<22 | minor<<12 | patch. Returns "" for 0, which is what
 * a driver that does not fill the field reports.
 */
fun decodeMesaDriverVersion(driverVersion: Long): String {
    if (driverVersion == 0L) return ""
    val major = (driverVersion shr 22) and 0x3FF
    val minor = (driverVersion shr 12) and 0x3FF
    val patch = driverVersion and 0xFFF
    return "$major.$minor.$patch"
}