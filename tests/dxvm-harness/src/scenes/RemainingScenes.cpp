// Scenes that are registered so the report is complete but are not ported yet.
//
// Every name from the DXML matrix that has no real implementation reports
// SKIP with an explicit reason. A report that silently omits a name would be
// worse than one that says "not ported yet": the harness contract is that
// every planned scene yields exactly one line.

#include "Registry.h"
#include "Report.h"

#include <string>
#include <vector>

namespace dxvmt {

  namespace {

    SceneResult notPorted() {
      return SceneResult::skip("not ported yet");
    }

    namespace compute {
      const char* const kNames[] = {
        "compute", "atomics", "append", "groupshared", "ballot32", "ballot32sync",
        "dispatchindirect", "asynccompile",
        "piece2bar", "pieceall", "pieceatomic", "piecebits", "piecedev", "pieceloop",
        "piecepadpred", "piecepaduncond", "pieceret", "pieceshared", "piecesink",
        "pieceslow", "pieceslow64", "pieceuav", "piecewarp0", "piecewarp3", "piecewarp6",
      };
    }

    namespace draw {
      const char* const kNames[] = {
        "drawindirect", "instancing", "geometry", "tessellation", "query", "copy",
        "alu", "internalpasses", "errstrides",
        "blend", "blendfactor", "blendprims", "depth", "depthclip", "depthclamp",
        "scissor", "frontface", "culldistance", "stencil", "primitives",
        "rendertarget", "discard", "discardsemantics", "errcull", "errclamp",
        "wireframecolor", "msaa",
      };
    }

    namespace texture {
      const char* const kNames[] = {
        "texops", "texarray", "texture3d", "cubemap", "mipmaps", "formats",
        "border", "gathertoffset", "shadowcompare", "mapnooverwrite",
        "rawbuffer", "typeduav",
      };
    }

  }

  void registerRemainingScenes(Registry& registry) {
    for (const char* n : compute::kNames)
      registry.add(n, &notPorted);

    for (const char* n : draw::kNames)
      registry.add(n, &notPorted);

    for (const char* n : texture::kNames)
      registry.add(n, &notPorted);
  }

}