#pragma once

#include "Scene.h"

#include <string>
#include <vector>

namespace dxvmt {

  /**
   * \brief Name to scene mapping, in registration order
   */
  class Registry {

  public:

    struct Entry {
      std::string name;
      SceneFn     fn;
    };

    void add(const std::string& name, SceneFn fn);

    const std::vector<Entry>& entries() const { return m_entries; }

    /**
     * \brief Select scenes by a comma separated name list
     *
     * An empty selection returns every entry. Registry order is preserved so a
     * report always lists scenes the same way. Unknown names are dropped; the
     * caller reports them.
     */
    std::vector<Entry> select(const std::string& csv) const;

  private:

    std::vector<Entry> m_entries;

  };

}