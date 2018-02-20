#ifndef ALLPLAY_STRINGGRAPH_H
#define ALLPLAY_STRINGGRAPH_H

#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/Twine.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/StringSaver.h>

#include <vector>

namespace allvm_analysis {

class StringGraph {
  using VertexID = size_t;
  using Edge = std::tuple<VertexID, VertexID, llvm::StringRef>;
  using NodeInfo = std::pair<llvm::StringRef, llvm::StringRef>;
  std::vector<Edge> Edges;
  llvm::StringMap<VertexID> StringIndexMap;
  llvm::BumpPtrAllocator Alloc;
  llvm::StringSaver Saver{Alloc};
  std::vector<NodeInfo> Nodes;

public:
  typedef std::pair<std::string, std::string> StringAttr;
  void addVertex(llvm::StringRef S, llvm::ArrayRef<StringAttr> attrs) {
    return addVertex(S, stringify(attrs));
  }
  void addVertexWithLabel(llvm::StringRef S, llvm::StringRef L) {
    StringAttr Attr{"label", L};
    return addVertex(S, Attr);
  }
  void addVertex(llvm::StringRef S, const llvm::Twine &Attrs) {
    assert(Nodes.size() == StringIndexMap.size());
    assert(!StringIndexMap.count(S));

    auto saved = Saver.save(S);
    StringIndexMap[saved] = Nodes.size();
    Nodes.push_back({saved, Saver.save(Attrs)});
  }

  auto getNodeIndex(llvm::StringRef N) {
    assert(StringIndexMap.count(N));
    return StringIndexMap[N];
  }
  auto getNodeAttrs(llvm::StringRef N) {
    // TODO: Put this in StringMap instead?
    return Nodes[getNodeIndex(N)].second;
  }

  void addEdge(llvm::StringRef A, llvm::StringRef B,
               llvm::ArrayRef<StringAttr> attrs) {
    return addEdge(A, B, stringify(attrs));
  }
  void addEdge(llvm::StringRef A, llvm::StringRef B,
               const llvm::Twine &Attrs = "") {
    auto V1 = getNodeIndex(A);
    auto V2 = getNodeIndex(B);

    Edges.push_back(Edge{V1, V2, Saver.save(Attrs)});
  }

  auto &nodes() const { return Nodes; }
  auto &edges() const { return Edges; }

  llvm::Error writeGraph(llvm::StringRef F);

private:
  std::string stringify(llvm::ArrayRef<StringAttr> attrs);
};

} // end namespace allvm_analysis

#endif // ALLPLAY_STRINGGRAPH_H
