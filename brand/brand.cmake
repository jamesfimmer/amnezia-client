set(CLIENT_TARGET_NAME "MyVPN" CACHE STRING "Client executable target name")
set(CLIENT_APPLICATION_NAME "MyVPN" CACHE STRING "Application display and executable name")
set(CLIENT_SERVICE_NAME "MyVPN-service" CACHE STRING "Service executable name")
set(CLIENT_ORGANIZATION_NAME "MyVPN.ORG" CACHE STRING "QSettings organization name")
set(CLIENT_APP_INSTANCE_NAME "MyVPNInstance" CACHE STRING "Single-instance local server name")

set(CLIENT_PROVIDER_API_URL "https://vpn.example.com" CACHE STRING "Provider API base URL")

if(CLIENT_PROVIDER_API_URL)
    add_compile_definitions(PROVIDER_API_URL="${CLIENT_PROVIDER_API_URL}")
endif()
