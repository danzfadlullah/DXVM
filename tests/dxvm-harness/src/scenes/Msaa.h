#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace dxvmt {

  /**
   * \brief Red value DXML expects from its per-sample ramp
   *
   * Pinned from the owner's real DXML run on Mali-G615 MC2 (driver r44p1),
   * where DXML's own engine passes all four and stock DXVK 3.1.1 fails the
   * 2x and 16x cases by reporting 26 for both. DXVM forks 3.1.1, so these are
   * the values the harness must hold it to.
   *
   * \returns the expected red channel, or -1 for a sample count with no
   *   pinned expectation.
   */
  int expectedSampleIndexRamp(int sampleCount);

  /** \brief One MSAA colour format the suite renders into */
  struct MsaaFormat {
    const char* suffix;  ///< scene-name suffix, e.g. "rgba8"
    int         format;  ///< DXGI_FORMAT value (resolved in the Windows TU)
  };

  /** \brief The three formats DXML covers for every sample count */
  std::vector<MsaaFormat> msaaFormats();

  /** \brief Partial-coverage pixels expected along the resolved diagonal */
  int msaaPartialPixelExpectation();

  /** \brief Expected (red, green) at pixel (10,20) after MSAA position resolve */
  std::pair<int, int> msaaPositionExpectation();

  /** \brief Expected "r,g,b" text from the Texture2DMS sample at 4x */
  std::string msaaTexture2dmsExpectation();

}