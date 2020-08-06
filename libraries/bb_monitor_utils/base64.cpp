#include "base64.h"
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <kspp/utils/spinlock.h>

static boost::uuids::random_generator random_gen;
static kspp::spinlock s_lockguard;

std::string encode64(const std::string &val) {
  using namespace boost::archive::iterators;
  using It = base64_from_binary<transform_width<std::string::const_iterator, 6, 8>>;
  auto tmp = std::string(It(std::begin(val)), It(std::end(val)));
  return tmp.append((3 - val.size() % 3) % 3, '=');
}

std::string encode64(const boost::uuids::uuid &val) {
  std::vector<uint8_t> v(val.data, val.data+16);
  return encode64(v);
}

std::string encode64(const std::vector<std::uint8_t>& data)
{
  using namespace boost::archive::iterators;
  using It = base64_from_binary<transform_width<std::vector<std::uint8_t>::const_iterator, 6, 8>>;
  std::string res(It(std::begin(data)), It(std::end(data)));
  return res.append((3 - data.size() % 3) % 3, '=');
}

std::string rnd_uuid_base64(){
  boost::uuids::uuid uuid;
  kspp::spinlock::scoped_lock xxx(s_lockguard);{
    uuid = random_gen();
  }
  return encode64(uuid);
}

//https://github.com/iotaledger/rpchub/pull/76/commits/ca26ea241b5230fb6e9493ed7bcfdfcc306bae94