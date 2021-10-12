hunter_config(bcos-framework VERSION 3.0.0-local
	URL https://${URL_BASE}/FISCO-BCOS/bcos-framework/archive/e2b7884a5da4f6a695a89660e75fe620aac4eca6.tar.gz
	SHA1 7979f68f6996134517df9f8e67ed6924f924eb6e
	CMAKE_ARGS HUNTER_PACKAGE_LOG_BUILD=ON HUNTER_PACKAGE_LOG_INSTALL=ON HUNTER_KEEP_PACKAGE_SOURCES=ON
)

hunter_config(bcos-crypto
    VERSION 3.0.0-local
    URL https://${URL_BASE}/FISCO-BCOS/bcos-crypto/archive/255002b047b359a45c953d1dab29efd2ff6eb080.tar.gz
    SHA1 4d02de20be1f9bf79d762c5b8686368286504e07
    CMAKE_ARGS URL_BASE=${URL_BASE}
)