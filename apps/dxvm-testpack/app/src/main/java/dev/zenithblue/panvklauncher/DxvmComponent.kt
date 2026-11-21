package dev.zenithblue.panvklauncher

import android.content.Context
import org.json.JSONObject
import java.io.File

/**
 * DXVM installed in PanPlay's DXVK component slot.
 *
 * The component contract (read from PanPlay's unpacker, see the spec):
 * tar+zstd, `profile.json` + `system32/` + `syswow64/` at the archive root, `type = "DXVK"`,
 * and for an arm64ec prefix `system32/` holds the ARM64EC DLLs while `syswow64/` holds the
 * i686 ones. PanPlay installs the archive to `filesDir/contents/DXVK/<versionName>/` and then
 * copies those DLLs into the Wine prefix.
 *
 * Why this file exists at all: PanPlay's DXVK slot can be filled by any `type: "DXVK"`
 * component, including its own upstream DXVK 3.1.1. Testing upstream DXVK while the report
 * says "DXVM" is this app's headline anti-goal, so every path here proves *which* component
 * is in play by requiring the packager's `system32/dxvm-marker.txt` **and** the `d3d11.dll`
 * that only an installed component carries. A marker alone would survive a partial copy; a
 * DLL alone is what upstream DXVK installs too.
 */
object DxvmComponent {
    const val TYPE = "DXVK"
    const val MARKER_NAME = "dxvm-marker.txt"
    const val D3D11 = "d3d11.dll"

    /** DLLs DXVM ships and the prefix must therefore receive. */
    val DLLS = listOf("d3d8.dll", "d3d9.dll", "d3d10core.dll", "d3d11.dll", "dxgi.dll")

    data class Profile(val type: String, val versionName: String, val description: String = "")

    /**
     * Validates a `profile.json` the way PanPlay's installer will use it, and rejects anything
     * that is not this app's DXVM DXVK component.
     *
     * Rules: `type` must be exactly `DXVK`; `versionName` must be present, non-blank, and usable
     * as a single path segment (the installer uses it as `contents/DXVK/<versionName>/`, so `.`
     * and `..` would escape and any character outside `[A-Za-z0-9._-]` is sanitised away).
     */
    fun validateProfile(json: String): Result<Profile> {
        val obj = try {
            JSONObject(json)
        } catch (t: Throwable) {
            return Result.failure(IllegalArgumentException("profile.json is not valid JSON: ${t.message}"))
        }
        val type = obj.optString("type", "")
        if (type != TYPE) {
            return Result.failure(IllegalArgumentException(
                "component type is '$type', expected '$TYPE'; refusing to install it as the DXVK slot"))
        }
        val raw = obj.optString("versionName", "")
        val version = raw.trim()
        if (version.isEmpty()) {
            return Result.failure(IllegalArgumentException("profile.json has no versionName"))
        }
        if (version == "." || version == "..") {
            return Result.failure(IllegalArgumentException("versionName '$version' is not a directory name"))
        }
        if (!version.all { it.isLetterOrDigit() || it == '.' || it == '-' || it == '_' }) {
            return Result.failure(IllegalArgumentException(
                "versionName '$version' contains characters the installer would strip"))
        }
        return Result.success(Profile(type, version, obj.optString("description", "")))
    }

    /** A directory holding a component's installed files (`contents/DXVK/<versionName>/`). */
    private fun system32(dir: File) = File(dir, "system32")

    /**
     * The version the packager stamped into `system32/dxvm-marker.txt`, or null when this
     * component did not come from DXVM's packager.
     */
    fun markerVersion(dir: File): String? {
        val marker = File(system32(dir), MARKER_NAME)
        if (!marker.isFile) return null
        return try {
            marker.readText().trim().ifEmpty { null }
        } catch (_: Exception) {
            null
        }
    }

    /**
     * True when [dir] is both a DXVM component *and* complete: the packager's marker plus the
     * `d3d11.dll` that actually carries the Direct3D implementation.
     */
    fun versionOf(dir: File): String? {
        val version = markerVersion(dir) ?: return null
        return if (File(system32(dir), D3D11).isFile) version else null
    }

    /**
     * True when the Wine prefix [windowsDir] (`drive_c/windows`) already carries DXVM. This is
     * the check the report depends on: it reads the prefix, not the component store, so a
     * component that was installed but never copied in cannot pass.
     */
    fun provesDxvmInPrefix(windowsDir: File): Boolean = versionOf(windowsDir) != null

    /** The version the prefix carries, or null when the prefix holds no DXVM marker. */
    fun prefixMarkerVersion(windowsDir: File): String? = markerVersion(windowsDir)

    /** `filesDir/container/.wine/drive_c/windows` — where Wine loads system32/syswow64 from. */
    fun windowsDir(ctx: Context): File =
        File(ctx.filesDir, "container/.wine/drive_c/windows")

    /** The installed DXVM component and its version, or null when only upstream DXVK is installed. */
    fun installed(ctx: Context): Pair<InstalledContent, String>? {
        for (c in ContentManager.list(ctx).filter { it.type == TYPE }) {
            val v = versionOf(c.dir) ?: continue
            return c to v
        }
        return null
    }

    /** Version of the DXVM component installed in the component store, or null. */
    fun installedVersion(ctx: Context): String? = installed(ctx)?.second

    /** Version the Wine prefix carries — what the report must name. */
    fun prefixVersion(ctx: Context): String? = prefixMarkerVersion(windowsDir(ctx))

    /**
     * Copies an installed DXVM component into the Wine prefix (`system32/` and `syswow64/`,
     * the marker included). Deterministic: it does not depend on which `type: "DXVK"` entry
     * PanPlay's own picker happens to choose first.
     */
    fun copyIntoPrefix(ctx: Context, componentDir: File): Result<Unit> {
        val windows = windowsDir(ctx)
        if (!File(windows, "system32").isDirectory) {
            return Result.failure(IllegalStateException(
                "Wine prefix is not set up yet (${windows.absolutePath}) — run the container setup first"))
        }
        return try {
            for (sub in listOf("system32", "syswow64")) {
                val src = File(componentDir, sub)
                if (!src.isDirectory) continue
                val dst = File(windows, sub).apply { mkdirs() }
                src.listFiles()?.forEach { f ->
                    if (f.isFile) f.copyTo(File(dst, f.name), overwrite = true)
                }
            }
            val version = versionOf(componentDir)
                ?: return Result.failure(IllegalStateException(
                    "component in ${componentDir.name} carries no DXVM marker"))
            if (!provesDxvmInPrefix(windows)) {
                return Result.failure(IllegalStateException(
                    "prefix still has no DXVM marker after copying DXVM $version"))
            }
            Result.success(Unit)
        } catch (t: Throwable) {
            Result.failure(IllegalStateException("could not copy DXVM into the prefix: ${t.message}", t))
        }
    }

    /**
     * Installs [wcp] as the DXVK component and, when the prefix already exists, copies it in.
     *
     * PanPlay's installer validates the archive's own `files[]` mapping and refuses an existing
     * `versionName`, which is why the packager stamps a per-build version. The DXVM proof is
     * checked on the *installed* directory before anything reaches the prefix: a component that
     * is not DXVM's must never become the DLLs Wine loads.
     */
    suspend fun install(ctx: Context, wcp: File): Result<String> {
        val installedResult = ContentManager.install(ctx, wcp)
        val content = installedResult.getOrElse { return Result.failure(it) }
        if (content.type != TYPE) {
            return Result.failure(IllegalStateException(
                "installed component is '${content.type}', expected '$TYPE'"))
        }
        val version = versionOf(content.dir)
            ?: return Result.failure(IllegalStateException(
                "component ${content.versionName} has no system32/$MARKER_NAME + $D3D11; " +
                    "it is not a DXVM component, so it was not put in the prefix"))
        copyIntoPrefix(ctx, content.dir).onFailure { return Result.failure(it) }
        return Result.success(version)
    }
}