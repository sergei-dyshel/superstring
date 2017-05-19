#include "test-helpers.h"
#include <sstream>
#include "text.h"
#include "text-slice.h"

using std::string;
using std::stringstream;
using std::vector;

TEST_CASE("Text::decode - can build a Text from a UTF8 stream") {
  string input = "abγdefg\nhijklmnop";
  auto conversion = Text::transcoding_from("UTF8");

  vector<char> encoding_buffer(3);
  stringstream stream(input, std::ios_base::in);

  vector<size_t> progress_reports;
  Text text;
  text.decode(*conversion, stream, encoding_buffer, [&](size_t percent_done) {
    progress_reports.push_back(percent_done);
  });

  REQUIRE(text == Text { u"abγdefg\nhijklmnop" });
  REQUIRE(progress_reports == vector<size_t>({2, 5, 8, 11, 14, 17, 18}));
}

TEST_CASE("Text::decode - replaces invalid byte sequences in the middle of the stream with the Unicode replacement character") {
  string input = "ab" "\xc0" "\xc1" "de";
  auto conversion = Text::transcoding_from("UTF8");

  vector<char> encoding_buffer(3);
  stringstream stream(input, std::ios_base::in);

  vector<size_t> progress_reports;
  Text text;
  text.decode(*conversion, stream, encoding_buffer, [&](size_t percent_done) {
    progress_reports.push_back(percent_done);
  });

  REQUIRE(text == Text { u"ab" "\ufffd" "\ufffd" "de" });
  REQUIRE(progress_reports == vector<size_t>({ 3, 6 }));
}

TEST_CASE("Text::decode - replaces invalid byte sequences at the end of the stream with the Unicode replacement characters") {
  string input = "ab" "\xf0\x9f"; // incomplete 4-byte code point for '😁' at the end of the stream
  auto conversion = Text::transcoding_from("UTF8");

  vector<char> encoding_buffer(5);
  stringstream stream(input, std::ios_base::in);

  Text text;
  text.decode(*conversion, stream, encoding_buffer, [&](size_t percent_done) {});
  REQUIRE(text == Text { u"ab" "\ufffd" "\ufffd" });
}

TEST_CASE("Text::decode - handles characters that require two 16-bit code units") {
  string input = "ab" "\xf0\x9f" "\x98\x81" "cd"; // 'ab😁cd'
  auto conversion = Text::transcoding_from("UTF8");

  vector<char> encoding_buffer(5);
  stringstream stream(input, std::ios_base::in);

  Text text;
  text.decode(*conversion, stream, encoding_buffer, [&](size_t percent_done) {});
  REQUIRE(text == Text { u"ab" "\xd83d" "\xde01" "cd" });
}

TEST_CASE("Text::decode - resizes the buffer if the encoding conversion runs out of room") {
  string input = "abcdef";
  auto conversion = Text::transcoding_from("UTF8");

  vector<char> encoding_buffer(5);
  stringstream stream(input, std::ios_base::in);

  Text text;
  text.decode(*conversion, stream, encoding_buffer, [&](size_t percent_done) {});
  REQUIRE(text == Text { u"abcdef" });
}

TEST_CASE("Text::decode - handles CRLF newlines") {
  string input = "abc\r\nde\rf\r\ng\r";
  auto conversion = Text::transcoding_from("UTF8");

  vector<char> encoding_buffer(4);
  stringstream stream(input, std::ios_base::in);

  Text text;
  text.decode(*conversion, stream, encoding_buffer, [&](size_t percent_done) {});
  REQUIRE(text == Text { u"abc\r\nde\rf\r\ng\r" });
}

TEST_CASE("Text::encode - basic") {
  Text text{u"abγdefg\nhijklmnop"};
  auto conversion = Text::transcoding_to("UTF8");
  vector<char> encoding_buffer(3);

  stringstream stream;
  text.encode(*conversion, 0, text.size(), stream, encoding_buffer);
  REQUIRE(stream.str() == "abγdefg\nhijklmnop");

  stringstream stream2;
  text.encode(*conversion, 1, text.size(), stream2, encoding_buffer);
  REQUIRE(stream2.str() == "bγdefg\nhijklmnop");
}

TEST_CASE("Text::encode - invalid characters") {
  Text text{u"abc" "\xD800" "def"};
  auto conversion = Text::transcoding_to("UTF8");
  vector<char> encoding_buffer(3);

  stringstream stream;
  text.encode(*conversion, 0, text.size(), stream, encoding_buffer);
  REQUIRE(stream.str() == "abc" "\ufffd" "def");

  stringstream stream2;
  text.encode(*conversion, 1, text.size(), stream2, encoding_buffer);
  REQUIRE(stream2.str() == "bc" "\ufffd" "def");

  stringstream stream3;
  text.encode(*conversion, 2, text.size(), stream3, encoding_buffer);
  REQUIRE(stream3.str() == "c" "\ufffd" "def");
}

TEST_CASE("Text::encode - invalid characters at the end of the slice") {
  Text text{u"abc" "\xD800"};
  auto conversion = Text::transcoding_to("UTF8");
  vector<char> encoding_buffer(3);

  stringstream stream;
  text.encode(*conversion, 0, text.size(), stream, encoding_buffer);
  REQUIRE(stream.str() == "abc" "\ufffd");

  stringstream stream2;
  text.encode(*conversion, 1, text.size(), stream2, encoding_buffer);
  REQUIRE(stream2.str() == "bc" "\ufffd");

  stringstream stream3;
  text.encode(*conversion, 2, text.size(), stream3, encoding_buffer);
  REQUIRE(stream3.str() == "c" "\ufffd");
}

TEST_CASE("Text::split") {
  Text text {u"abc\ndef\r\nghi"};
  TextSlice base_slice {text};

  {
    auto slices = base_slice.split({0, 2});
    REQUIRE(Text(slices.first) == Text(u"ab"));
    REQUIRE(Text(slices.second) == Text(u"c\ndef\r\nghi"));
  }

  {
    auto slices = base_slice.split({1, 2});
    REQUIRE(Text(slices.first) == Text(u"abc\nde"));
    REQUIRE(Text(slices.second) == Text(u"f\r\nghi"));
  }

  {
    auto slices = base_slice.split({1, 3});
    REQUIRE(Text(slices.first) == Text(u"abc\ndef"));
    REQUIRE(Text(slices.second) == Text(u"\r\nghi"));
  }

  {
    auto slices = base_slice.split({2, 0});
    REQUIRE(Text(slices.first) == Text(u"abc\ndef\r\n"));
    REQUIRE(Text(slices.second) == Text(u"ghi"));
  }

  {
    auto slices = base_slice.split({2, 3});
    REQUIRE(Text(slices.first) == Text(u"abc\ndef\r\nghi"));
    REQUIRE(Text(slices.second) == Text(u""));
  }
}

TEST_CASE("Text::concat") {
  Text text {u"abc\ndef\r\nghi"};
  TextSlice base_slice {text};

  REQUIRE(Text::concat(base_slice, base_slice) == Text(u"abc\ndef\r\nghiabc\ndef\r\nghi"));

  {
    auto prefix = base_slice.prefix({0, 2});
    auto suffix = base_slice.suffix({2, 2});
    REQUIRE(Text::concat(prefix, suffix) == Text(u"abi"));
  }

  {
    auto prefix = base_slice.prefix({1, 3});
    auto suffix = base_slice.suffix({2, 2});
    REQUIRE(Text::concat(prefix, suffix) == Text(u"abc\ndefi"));
  }

  {
    auto prefix = base_slice.prefix({1, 3});
    auto suffix = base_slice.suffix({2, 3});
    REQUIRE(Text::concat(prefix, suffix) == Text(u"abc\ndef"));
  }
}

TEST_CASE("Text::splice") {
  Text text {u"abc\ndef\r\nghi\njkl"};
  text.splice({1, 2}, {1, 1}, Text {u"mno\npq\r\nst"});
  REQUIRE(text == Text {u"abc\ndemno\npq\r\nsthi\njkl"});
  text.splice({2, 1}, {2, 1}, Text {u""});
  REQUIRE(text == Text {u"abc\ndemno\npkl"});
  text.splice({1, 1}, {0, 0}, Text {u"uvw"});
  REQUIRE(text == Text {u"abc\nduvwemno\npkl"});
  text.splice(text.extent(), {0, 0}, Text {u"\nxyz\r\nabc"});
  REQUIRE(text == Text {u"abc\nduvwemno\npkl\nxyz\r\nabc"});
  text.splice({0, 0}, {0, 0}, Text {u"def\nghi"});
  REQUIRE(text == Text {u"def\nghiabc\nduvwemno\npkl\nxyz\r\nabc"});
}

TEST_CASE("Text::offset_for_position - basic") {
  Text text {u"abc\ndefg\r\nhijkl"};

  REQUIRE(text.offset_for_position({0, 2}) == 2);
  REQUIRE(text.offset_for_position({0, 3}) == 3);
  REQUIRE(text.offset_for_position({0, 4}) == 3);
  REQUIRE(text.offset_for_position({0, 8}) == 3);

  REQUIRE(text.offset_for_position({1, 1}) == 5);
  REQUIRE(text.offset_for_position({1, 4}) == 8);
  REQUIRE(text.offset_for_position({1, 5}) == 8);
  REQUIRE(text.offset_for_position({1, 8}) == 8);

  REQUIRE(text.offset_for_position({2, 0}) == 10);
  REQUIRE(text.offset_for_position({2, 1}) == 11);
  REQUIRE(text.offset_for_position({2, 5}) == 15);
  REQUIRE(text.offset_for_position({2, 6}) == 15);
}

TEST_CASE("Text::offset_for_position - empty lines") {
  Text text {u"a\n\nb\r\rc"};
  TextSlice slice(text);

  REQUIRE(text.offset_for_position({0, 1}) == 1);
  REQUIRE(text.offset_for_position({0, 2}) == 1);
  REQUIRE(text.offset_for_position({0, UINT32_MAX}) == 1);
  REQUIRE(text.offset_for_position({1, 0}) == 2);
  REQUIRE(slice.position_for_offset(1) == Point(0, 1));
  REQUIRE(text.offset_for_position({1, 1}) == 2);
  REQUIRE(text.offset_for_position({1, UINT32_MAX}) == 2);
  REQUIRE(slice.position_for_offset(2) == Point(1, 0));
}
