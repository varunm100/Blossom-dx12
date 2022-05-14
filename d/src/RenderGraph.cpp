#include <algorithm>
#include <vector>

#include "d/RenderGraph.h"

namespace d {

  void RenderGraph::add_work(WorkPayload &&_payload, WorkType _type, std::initializer_list<Handle> _in_res,
                              std::initializer_list<Handle> _out_res) {
    nodes.push_back(std::make_shared<Work>(Work {
      .payload = _payload,
      .type = _type,
      .in_res = _in_res,
      .out_res = _out_res,
    }));
  }

  void RenderGraph::fill_links() {

  }

  void RenderGraph::add_works(std::initializer_list<Work> works) {
      for(const auto& w : works) {
        nodes.emplace_back(std::make_shared<Work>(w));
      }
  }
}
