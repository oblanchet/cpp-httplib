#include <future>
#include <gtest/gtest.h>
#include <httplib.h>

using namespace std;
using namespace httplib;

void ProxyTest(Client& cli, bool basic) {
  cli.set_proxy("localhost", basic ? 3128 : 3129);
  auto res = cli.Get("/get");
  ASSERT_TRUE(res != nullptr);
  EXPECT_EQ(407, res->status);
}

TEST(ProxyTest, NoSSLBasic) {
  Client cli("httpbin.org");
  ProxyTest(cli, true);
}

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
TEST(ProxyTest, SSLBasic) {
  SSLClient cli("httpbin.org");
  ProxyTest(cli, true);
}

TEST(ProxyTest, NoSSLDigest) {
  Client cli("httpbin.org");
  ProxyTest(cli, false);
}

TEST(ProxyTest, SSLDigest) {
  SSLClient cli("httpbin.org");
  ProxyTest(cli, false);
}
#endif

// ----------------------------------------------------------------------------

void RedirectProxyText(Client& cli, const char *path, bool basic) {
  cli.set_proxy("localhost", basic ? 3128 : 3129);
  if (basic) {
    cli.set_proxy_basic_auth("hello", "world");
  } else {
    cli.set_proxy_digest_auth("hello", "world");
  }
  cli.set_follow_location(true);

  auto res = cli.Get(path);
  ASSERT_TRUE(res != nullptr);
  EXPECT_EQ(200, res->status);
}

TEST(RedirectTest, HTTPBinNoSSLBasic) {
  Client cli("httpbin.org");
  RedirectProxyText(cli, "/redirect/2", true);
}

TEST(RedirectTest, HTTPBinNoSSLDigest) {
  Client cli("httpbin.org");
  RedirectProxyText(cli, "/redirect/2", false);
}

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
TEST(RedirectTest, HTTPBinSSLBasic) {
  SSLClient cli("httpbin.org");
  RedirectProxyText(cli, "/redirect/2", true);
}

TEST(RedirectTest, HTTPBinSSLDigest) {
  SSLClient cli("httpbin.org");
  RedirectProxyText(cli, "/redirect/2", false);
}
#endif

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
TEST(RedirectTest, YouTubeNoSSLBasic) {
  Client cli("youtube.com");
  RedirectProxyText(cli, "/", true);
}

TEST(RedirectTest, YouTubeNoSSLDigest) {
  Client cli("youtube.com");
  RedirectProxyText(cli, "/", false);
}

TEST(RedirectTest, YouTubeSSLBasic) {
  SSLClient cli("youtube.com");
  RedirectProxyText(cli, "/", true);
}

TEST(RedirectTest, YouTubeSSLDigest) {
  SSLClient cli("youtube.com");
  RedirectProxyText(cli, "/", false);
}
#endif

// ----------------------------------------------------------------------------

void BaseAuthTestFromHTTPWatch(Client& cli) {
  cli.set_proxy("localhost", 3128);
  cli.set_proxy_basic_auth("hello", "world");

  {
    auto res = cli.Get("/basic-auth/hello/world");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(401, res->status);
  }

  {
    auto res =
        cli.Get("/basic-auth/hello/world",
                {make_basic_authentication_header("hello", "world")});
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ("{\n  \"authenticated\": true, \n  \"user\": \"hello\"\n}\n", res->body);
    EXPECT_EQ(200, res->status);
  }

  {
    cli.set_basic_auth("hello", "world");
    auto res = cli.Get("/basic-auth/hello/world");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ("{\n  \"authenticated\": true, \n  \"user\": \"hello\"\n}\n", res->body);
    EXPECT_EQ(200, res->status);
  }

  {
    cli.set_basic_auth("hello", "bad");
    auto res = cli.Get("/basic-auth/hello/world");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(401, res->status);
  }

  {
    cli.set_basic_auth("bad", "world");
    auto res = cli.Get("/basic-auth/hello/world");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(401, res->status);
  }
}

TEST(BaseAuthTest, NoSSL) {
  Client cli("httpbin.org");
  BaseAuthTestFromHTTPWatch(cli);
}

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
TEST(BaseAuthTest, SSL) {
  SSLClient cli("httpbin.org");
  BaseAuthTestFromHTTPWatch(cli);
}
#endif

// ----------------------------------------------------------------------------

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
void DigestAuthTestFromHTTPWatch(Client& cli) {
  cli.set_proxy("localhost", 3129);
  cli.set_proxy_digest_auth("hello", "world");

  {
    auto res = cli.Get("/digest-auth/auth/hello/world");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(401, res->status);
  }

  {
    std::vector<std::string> paths = {
        "/digest-auth/auth/hello/world/MD5",
        "/digest-auth/auth/hello/world/SHA-256",
        "/digest-auth/auth/hello/world/SHA-512",
        "/digest-auth/auth-int/hello/world/MD5",
    };

    cli.set_digest_auth("hello", "world");
    for (auto path : paths) {
      auto res = cli.Get(path.c_str());
      ASSERT_TRUE(res != nullptr);
      EXPECT_EQ("{\n  \"authenticated\": true, \n  \"user\": \"hello\"\n}\n", res->body);
      EXPECT_EQ(200, res->status);
    }

    cli.set_digest_auth("hello", "bad");
    for (auto path : paths) {
      auto res = cli.Get(path.c_str());
      ASSERT_TRUE(res != nullptr);
      EXPECT_EQ(400, res->status);
    }

    cli.set_digest_auth("bad", "world");
    for (auto path : paths) {
      auto res = cli.Get(path.c_str());
      ASSERT_TRUE(res != nullptr);
      EXPECT_EQ(400, res->status);
    }
  }
}

TEST(DigestAuthTest, SSL) {
  SSLClient cli("httpbin.org");
  DigestAuthTestFromHTTPWatch(cli);
}

TEST(DigestAuthTest, NoSSL) {
  Client cli("httpbin.org");
  DigestAuthTestFromHTTPWatch(cli);
}
#endif

// ----------------------------------------------------------------------------

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
TEST(KeepAliveText, NoSSLWithDigest) {
  Client cli("httpbin.org");
  cli.set_keep_alive_max_count(4);
  cli.set_follow_location(true);
  cli.set_digest_auth("hello", "world");
  cli.set_proxy("localhost", 3129);
  cli.set_proxy_digest_auth("hello", "world");

  std::vector<Request> requests;

  Get(requests, "/get");
  Get(requests, "/redirect/2");
  Get(requests, "/digest-auth/auth/hello/world/MD5");

  std::vector<Response> responses;
  auto ret = cli.send(requests, responses);
  ASSERT_TRUE(ret == true);
  ASSERT_TRUE(requests.size() == responses.size());

  auto i = 0;

  {
    auto &res = responses[i++];
    EXPECT_EQ(200, res.status);
  }

  {
    auto &res = responses[i++];
    EXPECT_EQ(200, res.status);
  }

  {
    auto &res = responses[i++];
    EXPECT_EQ("{\n  \"authenticated\": true, \n  \"user\": \"hello\"\n}\n", res.body);
    EXPECT_EQ(200, res.status);
  }
}

TEST(KeepAliveText, SSLWithDigest) {
  SSLClient cli("httpbin.org");
  cli.set_keep_alive_max_count(4);
  cli.set_follow_location(true);
  cli.set_digest_auth("hello", "world");
  cli.set_proxy("localhost", 3129);
  cli.set_proxy_digest_auth("hello", "world");

  std::vector<Request> requests;

  Get(requests, "/get");
  Get(requests, "/redirect/2");
  Get(requests, "/digest-auth/auth/hello/world/MD5");

  std::vector<Response> responses;
  auto ret = cli.send(requests, responses);
  ASSERT_TRUE(ret == true);
  ASSERT_TRUE(requests.size() == responses.size());

  auto i = 0;

  {
    auto &res = responses[i++];
    EXPECT_EQ(200, res.status);
  }

  {
    auto &res = responses[i++];
    EXPECT_EQ(200, res.status);
  }

  {
    auto &res = responses[i++];
    EXPECT_EQ("{\n  \"authenticated\": true, \n  \"user\": \"hello\"\n}\n", res.body);
    EXPECT_EQ(200, res.status);
  }
}
#endif