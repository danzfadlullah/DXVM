#pragma once

#include "Scene.h"

#include <cstdint>
#include <string>
#include <vector>

namespace dxvmt {

  /**
   * \brief One report line for a scene result
   *
   * Format matches DXML: `<KIND> <name padded to column 15><detail>`, with a
   * minimum of one space so long names still separate cleanly.
   */
  std::string formatResult(const std::string& name, const SceneResult& result);

  /** \brief Section summary line: `SUMMARY <N> passed, <M> failed` */
  std::string formatSummary(int passed, int failed);

  /**
   * \brief Report header block
   *
   * Three lines plus a trailing blank line, per the real DXML report.
   */
  std::string formatHeader(
          const std::string& timestamp,
          const std::string& harnessVersion,
          int                code,
          int                passed,
          int                failed);

  /** \brief One row of the totals table: a label plus three passed/total pairs. */
  struct TotalsRow {
    std::string    label;
    int            passed0 = 0, total0 = 0;
    int            passed1 = 0, total1 = 0;
    int            passed2 = 0, total2 = 0;
  };

  /**
   * \brief Totals block
   *
   * Measured from the real report: `%-75s` label field, then three 11-wide
   * left-aligned columns.
   */
  std::string formatTotals(const std::vector<TotalsRow>& rows);

}