hunter_config(bcos-framework
	VERSION 3.0.0-local
	URL "https://${URL_BASE}/FISCO-BCOS/bcos-framework/archive/98a2530574ebf546fe38f92bddf3fe33305d9057.tar.gz"
	SHA1 be626ecd549151564dcec75cd4f2b2579188c237
)

hunter_config(bcos-crypto VERSION 3.0.0-local
        URL https://${URL_BASE}/FISCO-BCOS/bcos-crypto/archive/25c8edb7d5cbadb514bbce9733573c8ffdb3600d.tar.gz
        SHA1 4a1649e7095f5db58a5ae0671b2278bcccc25f1d
        CMAKE_ARGS HUNTER_PACKAGE_LOG_BUILD=ON HUNTER_PACKAGE_LOG_INSTALL=ON
)