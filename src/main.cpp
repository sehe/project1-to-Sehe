#include "stdafx.h"
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/date_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/thread/thread.hpp>
#include <iostream>
#include <stdio.h>
#include <string>
#include <unordered_map>
#include <boost/asio/lsr/router.h>

namespace asio = boost::asio;
namespace fs = boost::filesystem;
namespace sys = boost::system;
namespace bip = boost::interprocess;
using boost::asio::ip::tcp;
using boost::posix_time::ptime;

int send(char *host, unsigned short port, const char *file_path) {
    try {
        asio::io_service io_service_;
        tcp::socket socket_(io_service_, tcp::v4());
        sys::error_code error_code_;
        tcp::endpoint target_address(asio::ip::address_v4::from_string(host), port);
        socket_.connect(target_address);

        uintmax_t region_size = 5 * 1024 * 1024;
        uintmax_t file_size_  = fs::file_size(file_path);

        bip::file_mapping file_mapping_(file_path, bip::read_only);

        size_t written = 0;
        ptime start = boost::posix_time::microsec_clock::universal_time();
        for (uintmax_t position = 0; position < file_size_; position += region_size) {
            bip::mapped_region mapped_region_(
                    file_mapping_,
                    bip::read_only,
                    position,
                    std::min(region_size, file_size_ - position)
                );

            written += write(
                    socket_, 
                    asio::buffer(mapped_region_.get_address(), mapped_region_.get_size()),
                    asio::transfer_all(),
                    error_code_
                );
        }
        ptime finish = boost::posix_time::microsec_clock::universal_time();

        std::cout << "Sent " << written << " of " << file_size_ << " in " << finish - start << std::endl;
        socket_.close();
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}

int receive(unsigned short port, const char *file_name) {
    try {
        asio::io_service io_service_;
        tcp::socket socket_(io_service_);

        fs::path file_path(file_name);
        if (fs::exists(file_path)) {
            fs::remove(file_path);
        }

        const int buffer_size = 5 * 1024 * 1024;
        FILE *file            = fopen(file_name, "wb");
        char buffer_[buffer_size];

        tcp::endpoint   endpoint_(tcp::v4(),   port);
        tcp::acceptor   acceptor_(io_service_, endpoint_);
        sys::error_code error_code_;

        acceptor_.accept(socket_);
        std::cout << "Request from " << socket_.remote_endpoint() << "\n";

        size_t received = 0;
        size_t portion  = 0;
        ptime start     = boost::posix_time::microsec_clock::universal_time();
        do {
            portion   = read(socket_, asio::buffer(buffer_, buffer_size), error_code_);
            received += portion;

            fwrite(buffer_, 1, portion, file);
        } while (portion > 0);

        ptime finish = boost::posix_time::microsec_clock::universal_time();

        std::cout << "Received " << received << " in " << finish - start << std::endl;
        socket_.close();
        fclose(file);
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}

int check(char *file_name1, char *file_name2) {
    std::cout << "Checking ... ";
    std::cout.flush();

    FILE *file1 = fopen(file_name1, "rb");
    FILE *file2 = fopen(file_name2, "rb");
    const unsigned int buffer_size = 2 * 1024 * 1024;
    char buffer1[buffer_size];
    char buffer2[buffer_size];

    do {
        size_t red1 = fread(buffer1, 1, buffer_size, file1);
        size_t red2 = fread(buffer2, 1, buffer_size, file2);
        if (feof(file1) != feof(file2) || red1 != red2 || memcmp(buffer1, buffer2, red1)) {
            std::cout << "ERROR!!!" << std::endl;
            return -1;
        }
    } while (!feof(file1) && !feof(file2));

    std::cout << "OK" << std::endl;
    return 0;
}

int sendTest(char *destination, int port, int argc, char **argv) {
    asio::io_service io_service_;
    tcp::endpoint target_address(asio::ip::address_v4::from_string(destination), port);
    tcp::socket socket_(io_service_, tcp::v4());
    sys::error_code error_code_;

    if (argc > 0) {
        int route[argc + 1];
        ((char *)route)[0] = 1;
        ((char *)route)[1] = 131;
        ((char *)route)[2] = 3 + argc * 4;
        ((char *)route)[3] = 4;
        for (int i = 0; i < argc; i++) {
            route[i + 1] = inet_addr(argv[i]);
        }

        if (setsockopt(socket_.native_handle(), IPPROTO_IP, IP_OPTIONS, route, (argc + 1) * 4) < 0) {
            perror("can't set socket option");
        }
    }

    socket_.connect(target_address);
    std::cout << write(socket_, asio::buffer("Test", 4), error_code_) << std::endl;

    socket_.close();
    return 0;
}

int main2(int argc, char **argv) {
    if (argc >= 2) {
        std::string option(argv[1]);
        if (argc >= 5 && option.compare("-s") == 0) {
            return send(argv[2], static_cast<unsigned short>(atoi(argv[3])), argv[4]);
        } else if (argc >= 4 && option.compare("-r") == 0) {
            return receive(static_cast<unsigned short>(atoi(argv[2])), argv[3]);
        } else if (argc >= 4 && option.compare("-t") == 0) {
            return sendTest(argv[2], atoi(argv[3]), argc - 4, argv + 4);
        } else if (argc >= 4 && option.compare("-c") == 0) {
            return check(argv[2], argv[3]);
        }
    }
    std::cout << "Argument must be:" << std::endl << "\"-s HOST POST FILE_NAME\" for sender" << std::endl
         << "\"-r PORT FILE_NAME\" for receiver" << std::endl << "\"-c FILI_1_NAME FILE_2_NAME\" for receiver" << std::endl
         << "\"-t HOST PORT ROUTE_HOST_1 ...\" to test loose source send" << std::endl;
    return 1;
}

int main(int argc, char **argv) {
    std::unordered_multimap<unsigned int, unsigned int> graph {
        {1, 2}, {2, 1}, {1, 3}, {3, 1}, {3, 5}, {5, 3}, {2, 4}, {4, 2}, {1, 4},
        {4, 1}, {1, 7}, {7, 1}, {5, 6}, {6, 5}, {5, 7}, {7, 5}, {8, 7}, {7, 8},
        {4, 8}, {8, 4}, {8, 9}, {9, 8}, {9, 10}, {10, 9} };

    std::list<std::list<unsigned int> *> routes;

    // here the guess work starts
    typedef std::set<unsigned int, unsigned int> UIntSet;
    typedef std::map<unsigned int, unsigned int> UIntUIntMap;

    UIntUIntMap our_reverse;
    UIntUIntMap target_reverse;
    UIntSet intersections;

    boost::asio::lsr::router::findIntersections(graph, 1, 8, 4, intersections, our_reverse, target_reverse);
}