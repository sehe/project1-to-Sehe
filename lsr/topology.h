/* 
 * File:   Graph.h
 * Author: solozobov
 *
 * Created on 14 Апрель 2013 г., 23:58
 */

#ifndef TOPOLOGY_H
#define	TOPOLOGY_H

#include <boost/asio/lsr/typedef.h>

namespace boost {
namespace asio {
namespace lsr {
namespace topology {
	UIntUIntMultimap ip_graph;
	UIntUIntMultimap asn_graph;

	void addEdge(UIntUIntMultimap &graph, unsigned int id1, unsigned int id2) {
		bool notFound = true;
		std::pair<UIntUIntMultimapIterator, UIntUIntMultimapIterator> range = graph.equal_range(id1);
		for(UIntUIntMultimapIterator git = range.first; git != range.second; git++) {
			if (git->second == id2) {
				notFound = false;
				break;
			}
		}

		if (notFound) {
			graph.insert(UIntUIntMultimapEntry(id1, id2));
			graph.insert(UIntUIntMultimapEntry(id2, id1));
		}
	}

	void visit(UIntUIntMultimap &graph, void(*visitVertex)(unsigned int), void(*visitEdge)(unsigned int, unsigned int)) {
		UIntSet ids;
		bool isFirst = true;
		unsigned int lastVertex;
		for (UIntUIntMultimapIterator iterator = graph.begin(); iterator != graph.end(); ++iterator) {
			if (ids.find(iterator->first) == ids.end()) {
				ids.insert(iterator->first);
				visitVertex(iterator->first);
			}
			if (ids.find(iterator->second) == ids.end()) {
				ids.insert(iterator->second);
				visitVertex(iterator->second);
			}

			visitEdge(iterator->first, iterator->second);
		}
	}
} // namespace topology
} // namespace lsr
} // namespace asio
} // namespace boost

#endif	/* TOPOLOGY_H */