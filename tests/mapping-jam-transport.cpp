// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/ftebroom_jam_http.h"
#include <iostream>

int main(int argc, char** argv)
{
  if (argc != 2) return 2;
  if (ftebroom::jam::sha256("abc") != "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")
    return 1;
  namespace jam = ftebroom::jam;
  const std::string target = argv[1];
  const auto endpoint = target.find_first_not_of("0123456789") == target.npos
    ? jam::Endpoint{static_cast<unsigned>(std::stoul(target)),{}} : jam::httpsEndpoint(target);
  if (!endpoint.port) return 2;
  const auto response = jam::request(endpoint,"GET","/v1/jams/current",{},{},{},4096);
  if (response.status != 200 || response.body.find("local-first-jam") == std::string::npos) return 1;
  std::cout << "PASS: native " << (endpoint.hostname.empty() ? "loopback" : "HTTPS") << " transport and SHA-256\n";
}
