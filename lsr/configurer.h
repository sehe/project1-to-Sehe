/*
 * File:   IpAsnIndex.h
 * Author: solozobov
 *
 * Created on 6 Апрель 2013 г., 17:41
 */

#ifndef CONFIGURER_H
#define	CONFIGURER_H

#include <list>
#include <vector>
#include <sys/time.h>
#include <tr1/unordered_map>
#include <netinet/in.h> // sockaddr_in
#include <boost/asio/lsr/router.h>
#include <boost/thread/thread.hpp>

namespace boost {
namespace asio {
namespace lsr {
	using namespace std;
	using namespace std::tr1;
	
	const long REFRESH_ROUTES_TIMEOUT = 10 * 1000000;
	timeval last_route_update_time;

	static class Init {
		public:
			Init() {
				gettimeofday(&last_route_update_time, NULL);
				boost::thread routing_thread(router::run);
			}
	} init;

	long time_passed(timeval& time) {
		timeval current_time;
		gettimeofday(&current_time, NULL);
		return (current_time.tv_sec - time.tv_sec) * 1000000 + current_time.tv_usec - time.tv_usec;
	}
	
	struct LooseSourceOption {
		int length;
		char* bytes;
	};
	
	struct RouteOptions {
		int current = 0;
		vector<LooseSourceOption*> options;
	};
	
	unordered_set<unsigned int> last_targets;
	unordered_map<unsigned int, RouteOptions*> ip2routes;
	
	void updateRoutes() {
		if (time_passed(last_route_update_time) < REFRESH_ROUTES_TIMEOUT) {
			return;
		}
		
		router::guard.lock();
		
		timeval current_time;
		gettimeofday(&current_time, NULL);
		last_route_update_time = current_time;
		
		last_targets.swap(router::targets);
		last_targets.clear();
		
		for (unordered_map<unsigned int, RouteOptions*>::iterator it = ip2routes.begin(); it != ip2routes.end(); it++) {
			for (vector<LooseSourceOption*>::iterator lit = ((it->second)->options).begin(); lit != ((it->second)->options).end(); lit++) {
				delete (*lit)->bytes;
				delete (*lit);
			}
			delete (it->second);
		}
		
		for (unordered_map<unsigned int, list<router::route>* >::iterator it = router::ip2routes.begin(); it !=router::ip2routes.end(); it++) {
			RouteOptions* route_options = new RouteOptions();
			for (list<router::route>::iterator lit = (it->second)->begin(); lit != (it->second)->end(); lit++) {
				LooseSourceOption* loose_source_option = new LooseSourceOption();
				list<unsigned int>* route = *lit;
				loose_source_option->length = (route->size() - 1) * 4;
				loose_source_option->bytes = new char[loose_source_option->length];
				loose_source_option->bytes[0] = 1;
				loose_source_option->bytes[1] = 131;
				loose_source_option->bytes[2] = loose_source_option->length - 1;
				loose_source_option->bytes[3] = 4;
				list<unsigned int>::iterator it = route->begin();
				it++;
				for(int i = 1; i < route->size() - 1; i++, it++) {
					((unsigned int*) (loose_source_option->bytes))[i] = *it;
				}
				(route_options->options).push_back(loose_source_option);
			}
			ip2routes.insert(make_pair<unsigned int, RouteOptions*>(it->first, route_options));
		}

		router::guard.unlock();
	}

	void configure_route(int s) {
		updateRoutes();

		sockaddr_in addr;
		int addrLen = sizeof(addr);
		getpeername(s, (sockaddr *) &addr, (socklen_t*) &addrLen);
		//getsockname(s, (sockaddr *) &addr, (socklen_t*) &addrLen);
		unsigned int target_ip = addr.sin_addr.s_addr;
		last_targets.insert(target_ip);

		unordered_map<unsigned int, RouteOptions*>::iterator it = ip2routes.find(target_ip);
		if (it == ip2routes.end()) {
			if (setsockopt(s, IPPROTO_IP, IP_OPTIONS, NULL, 0) < 0) {
				perror("can't clear socket option");
			}
		} else {
			RouteOptions* options = it->second;
			LooseSourceOption* option = (options->options)[options->current];
			if (options->current ==  (options->options).size() - 1) {
				(options->current)++;
			} else {
				options->current = 0;
			}
			if (setsockopt(s, IPPROTO_IP, IP_OPTIONS, option->bytes, option->length) < 0) {
				perror("can't set socket option");
			}
		}
	}
} // namespace lsr
} // namespace asio
} // namespace boost

#endif