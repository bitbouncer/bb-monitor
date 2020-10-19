set -ef 

export CPP_STANDARD="17"

export AVRO_VER="release-1.10.0"
export AWS_SDK_VER="1.7.220"
export GRPC_VER="v1.32.0"

#deps for arrow
export DOUBLE_CONVERSION_VER="v3.1.5"
export BROTLI_VER="v1.0.9"
export FLATBUFFERS_VER="v1.11.0"
export THRIFT_VER="0.12.0"
export RAPIDJSON_VER="v1.1.0"

export ARROW_VER="apache-arrow-1.0.1"

export NLOHMANN_JSON_VER="3.9.1"
export PAHO_MQTT_C_VER="1.3.1"
export PAHO_MQTT_CPP_VER="1.0.1"
export ROCKDB_VER="v6.11.4"
export LIBRDKAFKA_VER="v1.5.0-RC1"
export PROMETHEUS_CPP_VER="v0.9.0"

export KSPP_VER="v0.9.1"

export CATCH2_VER="v2.11.3"
export RESTINIO_VER="v.0.6.0"
export HOWARD_HINNANT_VER="v3.0.0"

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib

rm -rf tmp
mkdir tmp
cd tmp

wget -O avro.tar.gz "https://github.com/apache/avro/archive/$AVRO_VER.tar.gz"
mkdir -p avro
tar \
  --extract \
  --file avro.tar.gz \
  --directory avro \
  --strip-components 1
sed -i.bak1 's/-std=c++11/-std=c++17/g' avro/lang/c++/CMakeLists.txt
sed -i.bak2 '/regex system)/a SET(Boost_LIBRARIES boost_program_options boost_iostreams boost_filesystem boost_regex boost_system z bz2)' avro/lang/c++/CMakeLists.txt
#sed -i.bak3 '/find_package (Boost/d' avro/lang/c++/CMakeLists.txt
#sed -i.bak4 '/regex system)/d' avro/lang/c++/CMakeLists.txt
cat avro/lang/c++/CMakeLists.txt
cd avro/lang/c++/ 
mkdir build 
cd build
cmake -DCMAKE_BUILD_TYPE=Release .. -DBUILD_SHARED_LIBS=ON -DCMAKE_CXX_STANDARD=$CPP_STANDARD
make -j "$(getconf _NPROCESSORS_ONLN)"
sudo make install
cd ../../../..

git clone --recursiv --depth 1 --branch $GRPC_VER https://github.com/grpc/grpc.git && \
cd grpc  && \
mkdir -p build && cd build  && \
cmake ..  && \
make -j "$(getconf _NPROCESSORS_ONLN)" && \
sudo make install && \
cd ../..

wget -O aws-sdk.tar.gz "https://github.com/aws/aws-sdk-cpp/archive/$AWS_SDK_VER.tar.gz" && \
mkdir -p aws-sdk && \
tar \
  --extract \
  --file aws-sdk.tar.gz \
  --directory aws-sdk \
  --strip-components 1  && \
cd aws-sdk && \
mkdir build-shared && \
cd build-shared && \
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DBUILD_ONLY="config;s3;transfer" -DENABLE_TESTING=OFF -DCPP_STANDARD=$CPP_STANDARD .. && \
make -j "$(getconf _NPROCESSORS_ONLN)"  && \
sudo make install && \
cd .. && \
mkdir build-static && \
cd build-static && \
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DBUILD_ONLY="config;s3;transfer" -DENABLE_TESTING=OFF -DCPP_STANDARD=$CPP_STANDARD .. && \
make -j "$(getconf _NPROCESSORS_ONLN)"  && \
sudo make install && \
cd ../..

wget -O double-conversion.tar.gz "https://github.com/google/double-conversion/archive/$DOUBLE_CONVERSION_VER.tar.gz" && \
mkdir -p double-conversion && \
tar \
  --extract \
  --file double-conversion.tar.gz \
  --directory double-conversion \
  --strip-components 1 && \
cd double-conversion && \
mkdir build && \
cd build && \
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DCMAKE_CXX_STANDARD=$CPP_STANDARD .. && \
make -j "$(getconf _NPROCESSORS_ONLN)" && \
sudo make install && \
cd ../..


wget -O brotli.tar.gz "https://github.com/google/brotli/archive/$BROTLI_VER.tar.gz" && \
mkdir -p brotli && \
tar \
  --extract \
  --file brotli.tar.gz \
  --directory brotli \
  --strip-components 1 && \
cd brotli && \
mkdir build && \
cd build && \
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DCMAKE_CXX_STANDARD=$CPP_STANDARD .. && \
make -j "$(getconf _NPROCESSORS_ONLN)" && \
sudo make install && \
cd ../..

wget -O flatbuffers.tar.gz "https://github.com/google/flatbuffers/archive/$FLATBUFFERS_VER.tar.gz" && \
mkdir -p flatbuffers && \
tar \
  --extract \
  --file flatbuffers.tar.gz \
  --directory flatbuffers \
  --strip-components 1 && \
cd flatbuffers && \
mkdir build && \
cd build && \
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DFLATBUFFERS_BUILD_TESTS=OFF -DCMAKE_CXX_STANDARD=$CPP_STANDARD .. && \
make -j "$(getconf _NPROCESSORS_ONLN)" && \
sudo make install && \
cd ../..


wget -O thrift.tar.gz "https://github.com/apache/thrift/archive/$THRIFT_VER.tar.gz" && \
mkdir -p thrift && \
tar \
  --extract \
  --file thrift.tar.gz \
  --directory thrift \
  --strip-components 1 && \
cd thrift && \
mkdir -p build && cd build && \
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DCMAKE_CXX_STANDARD=$CPP_STANDARD .. && \
make -j "$(getconf _NPROCESSORS_ONLN)" && \
sudo make install && \
cd ../..

wget -O rapidjson.tar.gz "https://github.com/miloyip/rapidjson/archive/$RAPIDJSON_VER.tar.gz" && \
mkdir -p rapidjson && \
tar \
   --extract \
   --file rapidjson.tar.gz \
   --directory rapidjson \
   --strip-components 1 && \
cd rapidjson && \
mkdir build && \
cd build && \
cmake -DRAPIDJSON_BUILD_EXAMPLES=OFF -DRAPIDJSON_BUILD_DOC=OFF -DRAPIDJSON_BUILD_TESTS=OFF -DBUILD_SHARED_LIBS=ON -DCMAKE_CXX_STANDARD=$CPP_STANDARD .. && \
sudo make install && \
sudo rm -rf /usr/local/share/doc/RapidJSON && \
cd ../..

wget -O arrow.tar.gz "https://github.com/apache/arrow/archive/$ARROW_VER.tar.gz" && \
mkdir -p arrow && \
tar \
  --extract \
  --file arrow.tar.gz \
  --directory arrow \
  --strip-components 1 && \
cd arrow/cpp && \
mkdir build && \
cd build && \
cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DARROW_DEPENDENCY_SOURCE=SYSTEM \
  -DCMAKE_CXX_STANDARD=$CPP_STANDARD \
  -DARROW_BUILD_UTILITIES=ON \
  -DARROW_CUDA=OFF \
  -DARROW_GANDIVA=ON \
  -DARROW_WITH_BZ2=ON \
  -DARROW_WITH_ZLIB=ON \
  -DARROW_WITH_ZSTD=ON \
  -DARROW_WITH_LZ4=ON \
  -DARROW_WITH_SNAPPY=ON \
  -DARROW_WITH_BROTLI=ON \
  -DARROW_COMPUTE=ON \
  -DARROW_JEMALLOC=ON \
  -DARROW_CSV=ON \
  -DARROW_DATASET=ON \
  -DARROW_FILESYSTEM=ON \
  -DARROW_JSON=ON \
  -DARROW_PARQUET=ON \
  -DARROW_PLASMA=ON \
  -DARROW_PYTHON=OFF \
  -DARROW_S3=ON \
  -DARROW_USE_GLOG=ON \
  -DPARQUET_BUILD_EXECUTABLES=ON \
  -DPARQUET_BUILD_EXAMPLES=ON \
   .. && \
make -j "$(getconf _NPROCESSORS_ONLN)" && \
sudo make install && \
cd ../../..

wget -O rocksdb.tar.gz "https://github.com/facebook/rocksdb/archive/$ROCKDB_VER.tar.gz" && \
mkdir -p rocksdb && \
tar \
    --extract \
    --file rocksdb.tar.gz \
    --directory rocksdb \
    --strip-components 1 && \
cd rocksdb && \
export USE_RTTI=1 && \
make -j "$(getconf _NPROCESSORS_ONLN)" shared_lib && \
sudo make install-shared && \
cd ..

wget -O prometheus-cpp.tar.gz "https://github.com/jupp0r/prometheus-cpp/archive/$PROMETHEUS_CPP_VER.tar.gz" && \
mkdir -p prometheus-cpp && \
tar \
  --extract \
  --file prometheus-cpp.tar.gz \
  --directory prometheus-cpp \
  --strip-components 1 && \
cd prometheus-cpp && \
mkdir build && cd build && \
cmake  -DCMAKE_BUILD_TYPE=Release -DENABLE_PULL=OFF -DUSE_THIRDPARTY_LIBRARIES=OFF -DENABLE_TESTING=OFF -DBUILD_SHARED_LIBS=ON -DOVERRIDE_CXX_STANDARD_FLAGS=OFF -DCMAKE_CXX_STANDARD=$CPP_STANDARD .. && \
make -j "$(getconf _NPROCESSORS_ONLN)" && \
sudo make install && \
cd ../..

wget -O librdkafka.tar.gz "https://github.com/edenhill/librdkafka/archive/$LIBRDKAFKA_VER.tar.gz" && \
mkdir -p librdkafka && \
tar \
  --extract \
  --file librdkafka.tar.gz \
  --directory librdkafka \
  --strip-components 1 && \
cd librdkafka && \
#./configure --prefix=/usr/local && \
./configure --disable-ssl --prefix=/usr/local && \
make -j "$(getconf _NPROCESSORS_ONLN)" && \
sudo make install && \
cd ..

wget -O nlomann.tar.gz "https://github.com/nlohmann/json/archive/$NLOHMANN_JSON_VER.tar.gz" && \
mkdir -p nlomann && \
tar \
  --extract \
  --file nlomann.tar.gz \
  --directory nlomann \
  --strip-components 1 && \
cd nlomann && \
mkdir build && cd build && \
cmake .. && \
make -j "$(getconf _NPROCESSORS_ONLN)" && \
sudo make install && \
cd ../..


wget -O paho.mqtt.c.tar.gz "https://github.com/eclipse/paho.mqtt.c/archive/v$PAHO_MQTT_C_VER.tar.gz" && \
mkdir -p paho.mqtt.c && \
tar \
  --extract \
  --file paho.mqtt.c.tar.gz \
  --directory paho.mqtt.c \
  --strip-components 1 && \
cd paho.mqtt.c && \
mkdir build && cd build  && \
cmake -DPAHO_WITH_SSL=ON -DPAHO_ENABLE_TESTING=OFF .. && \
make -j "$(getconf _NPROCESSORS_ONLN)" && \
sudo make install && \
cd ../.. 

wget -O paho.mqtt.cpp.tar.gz "https://github.com/eclipse/paho.mqtt.cpp/archive/v$PAHO_MQTT_CPP_VER.tar.gz" && \
mkdir -p paho.mqtt.cpp && \
tar \
  --extract \
  --file paho.mqtt.cpp.tar.gz \
  --directory paho.mqtt.cpp \
  --strip-components 1 && \
cd paho.mqtt.cpp && \
mkdir build && cd build && \
cmake -DPAHO_WITH_SSL=ON .. && \
make -j "$(getconf _NPROCESSORS_ONLN)" && \
sudo make install && \
cd ../.. 


wget -O hh.tar.gz "https://github.com/HowardHinnant/date/archive/$HOWARD_HINNANT_VER.tar.gz" && \
mkdir -p hh && \
tar \
  --extract \
  --file hh.tar.gz \
  --directory hh \
  --strip-components 1 && \
cd hh && \
mkdir build && cd build && \
cmake .. && \
make -j "$(getconf _NPROCESSORS_ONLN)" && \
sudo make install && \
cd ../..

wget -O catch2.tar.gz "https://github.com/catchorg/Catch2/archive/$CATCH2_VER.tar.gz" && \
mkdir -p catch2 && \
tar \
   --extract \
   --file catch2.tar.gz \
   --directory catch2 \
   --strip-components 1 && \
cd catch2 && \
mkdir build && cd build && \
cmake -DCATCH_BUILD_TESTING=OFF -DCATCH_INSTALL_DOCS=OFF .. && \
make -j "$(getconf _NPROCESSORS_ONLN)" && \
sudo make install && \
cd ../..

wget -O restinio.tar.gz "https://github.com/Stiffstream/restinio/archive/$RESTINIO_VER.tar.gz" && \
mkdir -p restinio && \
tar \
   --extract \
   --file restinio.tar.gz \
   --directory restinio \
   --strip-components 1 && \
cd restinio && \
cd dev && \
#sed -i.bak1 's/unofficial-http-parser/http-parser/g' CMakeLists.txt
sed -i.bak1 '/find_package(unofficial-http-parser/d' CMakeLists.txt && \
mkdir build && cd build && \
cmake -DRESTINIO_TEST=OFF -DRESTINIO_SAMPLE=OFF -DRESTINIO_INSTALL_SAMPLES=OFF -DRESTINIO_BENCH=OFF -DRESTINIO_INSTALL_BENCHES=OFF -DRESTINIO_FIND_DEPS=ON -DRESTINIO_ALLOW_SOBJECTIZER=OFF .. && \
make -j "$(getconf _NPROCESSORS_ONLN)" && \
sudo make install && \
cd ../../..

wget -O kspp.tar.gz "https://github.com/bitbouncer/kspp/archive/$KSPP_VER.tar.gz" && \
mkdir -p kspp && \
tar \
   --extract \
   --file kspp.tar.gz \
   --directory kspp \
   --strip-components 1 && \
cd kspp && \
rm -rf build && \ 
mkdir build && cd build && \
cmake -DENABLE_ROCKSDB=ON -DENABLE_POSTGRES=ON -DENABLE_INFLUXDB=ON -DENABLE_ELASTICSEARCH=ON -DENABLE_S3=ON -DENABLE_GRPC=ON -DBUILD_STATIC_LIBS=ON -DBUILD_SHARED_LIBS=OFF .. && \
make -j "$(getconf _NPROCESSORS_ONLN)" && \ 
sudo make install && \
cd ../..


#out of tmp
cd ..
rm -rf tmp



