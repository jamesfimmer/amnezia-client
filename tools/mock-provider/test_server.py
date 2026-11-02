import importlib.util
import json
import threading
import unittest
from http.server import ThreadingHTTPServer
from pathlib import Path
from urllib.error import HTTPError
from urllib.request import Request, urlopen


spec = importlib.util.spec_from_file_location("mock_provider", Path(__file__).with_name("server.py"))
provider = importlib.util.module_from_spec(spec)
spec.loader.exec_module(provider)


class ProviderApiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = ThreadingHTTPServer(("127.0.0.1", 0), provider.Handler)
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()
        cls.base = f"http://127.0.0.1:{cls.server.server_port}"

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join()

    def get(self, path, device="test-device"):
        request = Request(self.base + path, headers={"X-Device-Id": device})
        with urlopen(request, timeout=5) as response:
            self.assertEqual(response.headers.get_content_type(), "application/json")
            return json.load(response)

    def test_health(self):
        self.assertEqual(self.get("/v1/health"), {"status": "ok"})

    def test_unknown_route(self):
        with self.assertRaises(HTTPError) as caught:
            self.get("/missing")
        self.assertEqual(caught.exception.code, 404)
        caught.exception.close()

    def test_server_contract(self):
        servers = self.get("/v1/servers")["servers"]
        self.assertEqual(len(servers), 3)
        self.assertEqual(len({server["id"] for server in servers}), 3)
        for server in servers:
            self.assertTrue(server["name"])
            self.assertEqual(len(server["countryCode"]), 2)
            for field in ("[Interface]", "[Peer]", "PrivateKey =", "PublicKey =", "Endpoint =", "Jc ="):
                self.assertIn(field, server["config"])

    def test_same_device_is_stable(self):
        self.assertEqual(self.get("/v1/servers"), self.get("/v1/servers"))

    def test_devices_receive_different_configs(self):
        self.assertNotEqual(self.get("/v1/servers", "device-a"), self.get("/v1/servers", "device-b"))


if __name__ == "__main__":
    unittest.main()
