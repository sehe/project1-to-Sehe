/* 
 * File:   router.h
 * Author: solozobov
 *
 * Created on 15 Апрель 2013 г., 0:00
 */

#ifndef ROUTER_H
#define	ROUTER_H

//#define SEARCH_IN_AS

#include <time.h>
#include<list>
#include <vector>
#include <boost/signals2/mutex.hpp>
#include <boost/asio/lsr/topology.h>
#include <boost/asio/lsr/trace.h>
#include <boost/asio/lsr/typedef.h>

#include "asn_info.h"

#ifdef SEARCH_IN_AS
#include <boost/asio/lsr/asn_info.h>
#endif

namespace std {
	template<typename F, typename S>
	struct hash<pair<F, S> > {
		typedef pair<F, S> argument_type;
		typedef size_t result_type;
		
		result_type operator()(const argument_type& p) const {
			hash<F> hasher_first;
			hash<S> hasher_second;
			return hasher_first(p.first) * 31 + hasher_second(p.second);
		}
	};
	
} // namespace std

namespace boost {
namespace asio {
namespace lsr {
namespace router {
	
	const int IP_ROUTES_TO_SEARCH = 10;
	const int ASN_ROUTES_TO_SEARCH = 4;
	const int ROUTES_TO_SELECT = 2;
	const int TARGET_TRACE_COUNT = 10;
	const unsigned int OUR_IP = 0;
	const int OUR_ASN = -1;
	
	using namespace std;
	namespace s2 = boost::signals2;
	
	typedef list<unsigned int>* route;
	
    mutex guard;
	UIntSet targets;
	unordered_map<unsigned int, list<route>*> ip2routes; // routes contain source and destination
	
	int send_socket = trace::createUdpSocket();
	int recv_socket = trace::createIcmpSocket();
	
	void findNextGeneration(UIntUIntMultimap& graph, UIntSet& known,
	                        UIntSet& to_visit, UIntSet& discovered,
	                        UIntUIntMap& ancestor)
	{
		for (UIntSetIterator it = to_visit.begin(); it != to_visit.end(); it++) {
			pair<UIntUIntMultimapIterator, UIntUIntMultimapIterator> range = graph.equal_range(*it);
			for(UIntUIntMultimapIterator git = range.first; git != range.second; git++) {
				if (known.find(git->second) == known.end()) {
					discovered.insert(git->second);
					ancestor[git->second] = git->first;
				}
			}
		}
	}
	
	void updateIntersections(UIntSet& all, UIntSet& last, UIntSet& neww,
	                         UIntSet& intersections,
	                         UIntSet& another_last, UIntSet& another_all)
	{
		last.clear();
		for (UIntSetIterator it = neww.begin(); it != neww.end(); it++) {
			all.insert(*it);
			if (another_all.find(*it) == another_all.end()) {
				last.insert(*it);
			} else {
				intersections.insert(*it);
				another_last.erase(*it);
			}
		}
		neww.clear();
	}
	
	void findIntersections(UIntUIntMultimap& graph, unsigned int a, unsigned b, size_t intersections_to_search,
	                       UIntSet& intersections, UIntUIntMap& a_reverse, UIntUIntMap& b_reverse)
	{
		UIntSet a_all;
		UIntSet a_last;
		UIntSet a_new;
		UIntSet b_all;
		UIntSet b_last;
		UIntSet b_new;
		
		a_all.insert(a);
		a_last.insert(a);
		b_all.insert(b);
		b_last.insert(b);
		
		// searching routes doing BFS in turns from a and from b 
		// routes no longer then 9 edges are allowed
		for (int i = 0; i< 5; i++) {
			findNextGeneration(graph, a_all, a_last, a_new, a_reverse);
			// no intersections
			if (a_new.empty()) {
				break;
			}
			updateIntersections(a_all, a_last, a_new, intersections, b_last, b_all);
			
			findNextGeneration(graph, b_all, b_last, b_new, b_reverse);			
			// no intersections
			if (b_new.empty()) {
				break;
			}
			updateIntersections(b_all, b_last, b_new, intersections, a_last, a_all);
			
			if (intersections.size() >= intersections_to_search) {
				break;
			}
		}
	}
	
	void reverseRoutes(UIntSet& intersections, unsigned int a, unsigned b,
	                   UIntUIntMap& a_reverse, UIntUIntMap& b_reverse, list<route>& routes)
	{
		for (UIntSetIterator it = intersections.begin(); it != intersections.end(); it++) {
			route r = new list<unsigned int>();
			
			r->push_front(*it);
			
			unsigned int vertex = *it;
			do {
				vertex = a_reverse[vertex];
				r->push_front(vertex);
			} while (vertex != a);
			
			vertex = *it;
			do {
				vertex = b_reverse[vertex];
				r->push_back(vertex);
			} while (vertex != b);
			
			routes.push_back(r);
		}
	}
	
	void findRoutesIP(unsigned int targetIP, list<route>& routes) {
		UIntUIntMap our_reverse;
		UIntUIntMap target_reverse;
		UIntSet intersections;
		
		findIntersections(topology::ip_graph, OUR_IP, targetIP, IP_ROUTES_TO_SEARCH, intersections,
		                  our_reverse, target_reverse);
		reverseRoutes(intersections, OUR_IP, targetIP, our_reverse, target_reverse, routes);
	}
	
#ifdef SEARCH_IN_AS
	void findNextGenerationRestrictedASN(UIntUIntMultimap& graph, UIntSet& known,
	                                     UIntSet& to_visit, UIntSet& discovered,
	                                     UIntUIntMap& ancestor, UIntSet& permittedASN)
	{
		for (UIntSetIterator it = to_visit.begin(); it != to_visit.end(); it++) {
			pair<UIntUIntMultimapIterator, UIntUIntMultimapIterator> range = graph.equal_range(*it);
			for(UIntUIntMultimapIterator git = range.first; git != range.second; git++) {
				int asn_info_index = asn::getAsnInfo(git->second);
				if (known.find(git->second) == known.end() && asn_info_index != -1
				    && permittedASN.find(asn::asnInfo[asn_info_index].asn) != permittedASN.end())
				{
					discovered.insert(git->second);
					ancestor[git->second] = git->first;
				}
			}
		}
	}
		
	void findIntersectionsRestrictedASN(UIntUIntMultimap& graph, unsigned int a, unsigned b,
	                                    int intersections_to_search, UIntSet& intersections, UIntSet& permittedASN,
	                                    UIntUIntMap& a_reverse, UIntUIntMap& b_reverse)
	{
		UIntSet a_all;
		UIntSet a_last;
		UIntSet a_new;
		UIntSet b_all;
		UIntSet b_last;
		UIntSet b_new;
		
		a_all.insert(a);
		a_last.insert(a);
		b_all.insert(b);
		b_last.insert(b);
		
		// searching routes doing BFS in turns from a and from b 
		// routes no longer then 9 edges are allowed
		for (int i = 0; i< 5; i++) {
			findNextGenerationRestrictedASN(graph, a_all, a_last, a_new, a_reverse, permittedASN);
			// no intersections
			if (a_new.empty()) {
				break;
			}
			updateIntersections(a_all, a_last, a_new, intersections, b_last);
			
			findNextGenerationRestrictedASN(graph, b_all, b_last, b_new, b_reverse, permittedASN);			
			// no intersections
			if (b_new.empty()) {
				break;
			}
			updateIntersections(b_all, b_last, b_new, intersections, a_last);
			
			if (intersections.size() >= intersections_to_search) {
				break;
			}
		}
	}

	void findRoutesASN(unsigned int targetIP, list<route>& routes) {
		int asn_info_index = asn::getAsnInfo(targetIP);
		if (asn_info_index == -1) {
			findRoutesIP(targetIP, routes);
			return;
		}
		unsigned int targetASN = asn::asnInfo[asn_info_index].asn;
		
		UIntUIntMap our_reverse;
		UIntUIntMap target_reverse;
		UIntSet intersections;
		
		findIntersections(topology::ip_graph, OUR_ASN, targetASN, ASN_ROUTES_TO_SEARCH, intersections, our_reverse, target_reverse);
		reverseRoutes(intersections, OUR_ASN, targetASN, our_reverse, target_reverse, routes);
		
		UIntSet permittedASN;
		for (list<route>::iterator rit = routes.begin(); rit != routes.end(); rit++) {
			for (list<unsigned int>::iterator it = (*rit)->begin(); it != (*rit)->end(); it++) {
				permittedASN.insert(*it);
			}
			delete *rit;
		}
		routes.clear();
		
		findIntersectionsRestrictedASN(topology::ip_graph, OUR_IP, targetIP, IP_ROUTES_TO_SEARCH, intersections,
		                               permittedASN, our_reverse, target_reverse);
		reverseRoutes(intersections, OUR_IP, targetIP, our_reverse, target_reverse, routes);
		
	}
#endif
	
		bool validateAndNormalizeRoute(vector<unsigned int>& route) {
		if (route.size() == 0) {
			return false;
		}
		int compressedRouteLength = 0;
		
		// remove repeated IPs and unknown hosts
		unsigned int lastIp = 0;
		for (size_t i = 0; i < route.size(); i++) {
			unsigned int newIp = route[i];
			if (newIp != 0 && newIp != lastIp) {
				route[compressedRouteLength++] = newIp;
				lastIp = newIp;
			}
		}
		route.resize(compressedRouteLength);
		
		// decline possibly cyclic routes
		IntSet allIps;
		for (size_t i = 0; i < route.size(); i++) {
			unsigned int ip = route[i];
			if (ip != 0) {
				if (allIps.count(ip)) {
					return false;
				}
				allIps.insert(ip);
			}
		}
		return true;
	}
	
	void storeRoute(vector<unsigned int>& route) {
		int lastIP = OUR_IP;
		//int lastASN = OUR_ASN;
		for (size_t i = 0; i < route.size(); i++) {
			int nextIP = route[i];
			topology::addEdge(topology::ip_graph, lastIP, nextIP);
			lastIP = nextIP;
#ifdef SEARCH_IN_AS
			int asn_info_index = asn::getAsnInfo(nextIP);
			int nextASN;
			if (asn_info_index != -1) {
				nextASN = asn::asnInfo[asn_info_index].asn;
			} else {
				nextASN = -1;
			}
			if (lastASN != -1 && nextASN != -1 && nextASN != lastASN) {
				topology::addEdge(topology::asn_graph, lastASN, nextASN);
			}
			lastASN = nextASN;
#endif
		}
	}
	
	bool compareRoutesQuality(pair<double, route> p1, pair<double, route> p2) {
		return p1.first < p2.first;
	}
	
	void selectBestRoutes(unsigned int targetIP, unsigned num, list<route>& routes) {
		vector<unsigned int> standard_route;
		trace::traceRoute(targetIP, NULL, 0, send_socket, recv_socket, standard_route);
		if (validateAndNormalizeRoute(standard_route)) {
			storeRoute(standard_route);
		} else {
			sockaddr_in address;
			address.sin_addr.s_addr = targetIP;
			printf("Can't trace standard route to %s\n", inet_ntoa(address.sin_addr));
			routes.clear();
			return;
		}
		
		int standard_route_length = standard_route.size();
		unordered_set<pair<unsigned int, unsigned int> > standard_route_edges;
		unsigned int lastIP = OUR_IP;
		for (size_t i = 0; i < standard_route.size(); i++) {
			unsigned int nextIP = standard_route[i];
			standard_route_edges.insert(std::make_pair(lastIP, nextIP));
			standard_route_edges.insert(std::make_pair(nextIP, lastIP));
			lastIP = nextIP;
		}
		
		// ordering routes by quality
		list<pair<double, route> > quality_of_routes;
		for (list<route>::iterator rit = routes.begin(); rit != routes.end(); rit++) {
			int common_edges = 0;
			list<unsigned int>::iterator it = (*rit)->begin();
			unsigned int lastIP = *it;
			it++;
			while(it != (*rit)->end()) {
				unsigned int nextIP = *it;
				if (standard_route_edges.find(pair<unsigned int, unsigned int>(lastIP, nextIP)) != standard_route_edges.end()){
					common_edges++;
				}
				lastIP = nextIP;
			}
			
			quality_of_routes.push_back(std::make_pair(
                        static_cast<double>(common_edges) * standard_route_length / (*rit)->size(), 
                        *rit));
		}
		
		unsigned i = 0;
		for(list<pair<double, route> >::iterator it = quality_of_routes.begin(); i < num && i < quality_of_routes.size(); i++) {
			routes.push_front(it->second);
		}
		routes.resize(i);
	}
	
	void updateRoutes() {
		guard.lock();
		for (unordered_map<unsigned int, list<route>*>::iterator it = ip2routes.begin(); it != ip2routes.end(); it++) {
			for (list<route>::iterator lit = (it->second)->begin(); lit != (it->second)->end(); lit++) {
				delete (*lit);
			}
			delete (it->second);
		}
		ip2routes.clear();

		for (unordered_set<unsigned int>::iterator target = targets.begin(); target != targets.end(); target++) {
			list<route>* routes = new list<route>();
#ifdef SEARCH_IN_AS
			findRoutesASN(*target, *routes);
#else
			findRoutesIP(*target, *routes);
#endif
			if (!routes->empty()) {
				selectBestRoutes(*target, 2, *routes);
				ip2routes.insert(std::make_pair(*target, routes));
			} else {
				delete routes;
			}
		}
		guard.unlock();
	}
	
	void traceRoutes() {
		guard.lock();
		UIntSet trace_targets(targets); // скопируется и ли переместится?
		guard.unlock();
		
		vector<unsigned int> r;
		for (UIntSetIterator it = trace_targets.begin(); it != trace_targets.end(); it++) {
#ifdef SEARCH_IN_AS
			int asn_info_index = asn::getAsnInfo(*it);
#endif
			for (int i = 0; i < TARGET_TRACE_COUNT; i++) {
				unsigned int lsr = rand();
#ifdef SEARCH_IN_AS
				if (asn_info_index != -1) {
					asn::AsnInfo asn_info = asn::asnInfo[asn_info_index];
					lsr = asn_info.start_ip + (lsr % (asn_info.end_ip - asn_info.start_ip + 1));
				}
#endif
				trace::traceRoute(*it, &lsr, 1, send_socket, recv_socket, r);
				if (validateAndNormalizeRoute(r)) {
					storeRoute(r);
				}
				r.clear();
			}
		}
	}
	
	void run() {
#ifdef SEARCH_IN_AS
		asn::init();
#endif
		srand(time(NULL));
		while (true) {
			traceRoutes();
			updateRoutes();
		}
	}
} // namespace router
} // namespace lsr
} // namespace asio
} // namespace boost

#endif	/* ROUTER_H */

