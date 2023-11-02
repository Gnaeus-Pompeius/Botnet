#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <errno.h>
#include <vector>
#include <string>
#include <iostream>

std::vector<std::string> get_interfaces(bool include_ipv6 = false, bool include_loopback = true) {
	struct ifaddrs *myaddrs, *ifa;
	void *in_addr;
	char buf[64];

	std::vector<std::string> addresses;

	if(getifaddrs(&myaddrs) != 0)
	{
		// error - we might want to exit the program here?
		return addresses;
	}

	for (ifa = myaddrs; ifa != NULL; ifa = ifa->ifa_next)
	{
		if (ifa->ifa_addr == NULL)
			continue;
		if (!(ifa->ifa_flags & IFF_UP))
			continue;

		switch (ifa->ifa_addr->sa_family)
		{
			case AF_INET:
			{
				struct sockaddr_in *s4 = (struct sockaddr_in *)ifa->ifa_addr;
				in_addr = &s4->sin_addr;
				break;
			}

			case AF_INET6:
			{
				struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)ifa->ifa_addr;
				in_addr = &s6->sin6_addr;
				break;
			}

			default:
				continue;
		}

		bool is_ipv6 = (ifa->ifa_addr->sa_family == AF_INET6);

		if (inet_ntop(ifa->ifa_addr->sa_family, in_addr, buf, sizeof(buf))) {
			bool is_loopback = ifa->ifa_flags & IFF_LOOPBACK;
			if ( (include_loopback || !is_loopback) && (include_ipv6 || !is_ipv6) ) {
				addresses.push_back(buf);
			}
		}
	}

	freeifaddrs(myaddrs);

	return addresses;
}

int main(int argc, char *argv[])
{

	std::vector<std::string> addresses = get_interfaces();

	std::cout << "Addresses:" << std::endl;
	for(const auto& iter : addresses) {
		std::cout << iter << std::endl;
	}

	return 0;
}
