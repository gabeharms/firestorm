/** 
 * @file llsdserialize_test.cpp
 * @date 2006-04
 * @brief LLSDSerialize unit tests
 *
 * $LicenseInfo:firstyear=2006&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */


#include "linden_common.h"

#if LL_WINDOWS
#include <winsock2.h>
typedef U32 uint32_t;
#include <process.h>
#include <io.h>
#else
#include <unistd.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/wait.h>
#include "llprocesslauncher.h"
#endif

#include <fcntl.h>
#include <sys/stat.h>

/*==========================================================================*|
// Whoops, seems Linden's Boost package and the viewer are built with
// different settings of VC's /Zc:wchar_t switch! Using Boost.Filesystem
// pathname operations produces Windows link errors:
// unresolved external symbol "private: static class std::codecvt<unsigned short,
// char,int> const * & __cdecl boost::filesystem3::path::wchar_t_codecvt_facet()"
// unresolved external symbol "void __cdecl boost::filesystem3::path_traits::convert()"
// See:
// http://boost.2283326.n4.nabble.com/filesystem-v3-unicode-and-std-codecvt-linker-error-td3455549.html
// which points to:
// http://msdn.microsoft.com/en-us/library/dh8che7s%28v=VS.100%29.aspx

// As we're not trying to preserve compatibility with old Boost.Filesystem
// code, but rather writing brand-new code, use the newest available
// Filesystem API.
#define BOOST_FILESYSTEM_VERSION 3
#include "boost/filesystem.hpp"
#include "boost/filesystem/v3/fstream.hpp"
|*==========================================================================*/
#include "boost/range.hpp"
#include "boost/foreach.hpp"

#include "../llsd.h"
#include "../llsdserialize.h"
#include "../llformat.h"

#include "../test/lltut.h"
#include "stringize.h"

std::vector<U8> string_to_vector(const std::string& str)
{
	return std::vector<U8>(str.begin(), str.end());
}

// boost::filesystem::temp_directory_path() isn't yet in Boost 1.45! :-(
std::string temp_directory_path()
{
#if LL_WINDOWS
    char buffer[4096];
    GetTempPathA(sizeof(buffer), buffer);
    return buffer;

#else  // LL_DARWIN, LL_LINUX
    static const char* vars[] = { "TMPDIR", "TMP", "TEMP", "TEMPDIR" };
    BOOST_FOREACH(const char* var, vars)
    {
        const char* found = getenv(var);
        if (found)
            return found;
    }
    return "/tmp";
#endif // LL_DARWIN, LL_LINUX
}

// Windows presents a kinda sorta compatibility layer. Code to the yucky
// Windows names because they're less likely than the Posix names to collide
// with any other names in this source.
#if LL_WINDOWS
#define _remove   DeleteFileA
#else  // ! LL_WINDOWS
#define _open     open
#define _write    write
#define _close    close
#define _remove   remove
#define _O_WRONLY O_WRONLY
#define _O_CREAT  O_CREAT
#define _O_EXCL   O_EXCL
#define _S_IWRITE S_IWUSR
#define _S_IREAD  S_IRUSR
#endif  // ! LL_WINDOWS

// Create a Python script file with specified content "somewhere in the
// filesystem," cleaning up when it goes out of scope.
class NamedTempScript
{
public:
    NamedTempScript(const std::string& content):
        mPath(temp_directory_path())
    {
        // Make sure mPath ends with a directory separator, if it doesn't already.
        if (mPath.empty() ||
            ! (mPath[mPath.length() - 1] == '\\' || mPath[mPath.length() - 1] == '/'))
        {
            mPath.append("/");
        }

        // Open a file with a unique name in the mPath directory.
        int fd(-1);
        for (int i(0);; ++i)
        {
            // Append an integer name to mPath. It need not be zero-filled,
            // but I think it's neater that way.
            std::string testname(STRINGIZE(mPath
                                           << std::setw(8) << std::setfill('0') << i
                                           << ".py"));
            // The key to this whole loop is the _O_CREAT | _O_EXCL bitmask,
            // which requests an error if the file already exists. A proper
            // implementation will check atomically, ensuring that racing
            // processes will end up with two different filenames.
            fd = _open(testname.c_str(),
                       _O_WRONLY | _O_CREAT | _O_EXCL,
                       _S_IREAD | _S_IWRITE);
            if (fd != -1)           // managed to open the file
            {
                mPath = testname;   // remember its actual name
                break;
            }
            // it's a problem if the open() failed for any other reason but
            // the existence of a file by the same name
            assert(errno == EEXIST);
            // loop back to try another filename
        }
        // File is open, its name is in mPath: write it and close it.
        (void)_write(fd, content.c_str(), content.length());
        (void)_write(fd, "\n", 1);
        _close(fd);
    }

    ~NamedTempScript()
    {
        _remove(mPath.c_str());
    }

    std::string getName() const { return mPath; }

private:
    std::string mPath;
};

namespace tut
{
	struct sd_xml_data
	{
		sd_xml_data()
		{
			mFormatter = new LLSDXMLFormatter;
		}
		LLSD mSD;
		LLPointer<LLSDXMLFormatter> mFormatter;
		void xml_test(const char* name, const std::string& expected)
		{
			std::ostringstream ostr;
			mFormatter->format(mSD, ostr);
			ensure_equals(name, ostr.str(), expected);
		}
	};

	typedef test_group<sd_xml_data> sd_xml_test;
	typedef sd_xml_test::object sd_xml_object;
	tut::sd_xml_test sd_xml_stream("LLSDXMLFormatter");

	template<> template<>
	void sd_xml_object::test<1>()
	{
		// random atomic tests
		std::string expected;

		expected = "<llsd><undef /></llsd>\n";
		xml_test("undef", expected);

		mSD = 3463;
		expected = "<llsd><integer>3463</integer></llsd>\n";
		xml_test("integer", expected);

		mSD = "";
		expected = "<llsd><string /></llsd>\n";
		xml_test("empty string", expected);

		mSD = "foobar";
		expected = "<llsd><string>foobar</string></llsd>\n";
		xml_test("string", expected);

		mSD = LLUUID::null;
		expected = "<llsd><uuid /></llsd>\n";
		xml_test("null uuid", expected);
		
		mSD = LLUUID("c96f9b1e-f589-4100-9774-d98643ce0bed");
		expected = "<llsd><uuid>c96f9b1e-f589-4100-9774-d98643ce0bed</uuid></llsd>\n";
		xml_test("uuid", expected);

		mSD = LLURI("https://secondlife.com/login");
		expected = "<llsd><uri>https://secondlife.com/login</uri></llsd>\n";
		xml_test("uri", expected);

		mSD = LLDate("2006-04-24T16:11:33Z");
		expected = "<llsd><date>2006-04-24T16:11:33Z</date></llsd>\n";
		xml_test("date", expected);

		// Generated by: echo -n 'hello' | openssl enc -e -base64
		std::vector<U8> hello;
		hello.push_back('h');
		hello.push_back('e');
		hello.push_back('l');
		hello.push_back('l');
		hello.push_back('o');
		mSD = hello;
		expected = "<llsd><binary encoding=\"base64\">aGVsbG8=</binary></llsd>\n";
		xml_test("binary", expected);
	}
	
	template<> template<>
	void sd_xml_object::test<2>()
	{
		// tests with boolean values.
		std::string expected;

		mFormatter->boolalpha(true);
		mSD = true;
		expected = "<llsd><boolean>true</boolean></llsd>\n";
		xml_test("bool alpha true", expected);
		mSD = false;
		expected = "<llsd><boolean>false</boolean></llsd>\n";
		xml_test("bool alpha false", expected);

		mFormatter->boolalpha(false);
		mSD = true;
		expected = "<llsd><boolean>1</boolean></llsd>\n";
		xml_test("bool true", expected);
		mSD = false;
		expected = "<llsd><boolean>0</boolean></llsd>\n";
		xml_test("bool false", expected);
	}


	template<> template<>
	void sd_xml_object::test<3>()
	{
		// tests with real values.
		std::string expected;

		mFormatter->realFormat("%.2f");
		mSD = 1.0;
		expected = "<llsd><real>1.00</real></llsd>\n";
		xml_test("real 1", expected);

		mSD = -34379.0438;
		expected = "<llsd><real>-34379.04</real></llsd>\n";
		xml_test("real reduced precision", expected);
		mFormatter->realFormat("%.4f");
		expected = "<llsd><real>-34379.0438</real></llsd>\n";
		xml_test("higher precision", expected);

		mFormatter->realFormat("%.0f");
		mSD = 0.0;
		expected = "<llsd><real>0</real></llsd>\n";
		xml_test("no decimal 0", expected);
		mSD = 3287.4387;
		expected = "<llsd><real>3287</real></llsd>\n";
		xml_test("no decimal real number", expected);
	}

	template<> template<>
	void sd_xml_object::test<4>()
	{
		// tests with arrays
		std::string expected;

		mSD = LLSD::emptyArray();
		expected = "<llsd><array /></llsd>\n";
		xml_test("empty array", expected);

		mSD.append(LLSD());
		expected = "<llsd><array><undef /></array></llsd>\n";
		xml_test("1 element array", expected);

		mSD.append(1);
		expected = "<llsd><array><undef /><integer>1</integer></array></llsd>\n";
		xml_test("2 element array", expected);
	}

	template<> template<>
	void sd_xml_object::test<5>()
	{
		// tests with arrays
		std::string expected;

		mSD = LLSD::emptyMap();
		expected = "<llsd><map /></llsd>\n";
		xml_test("empty map", expected);

		mSD["foo"] = "bar";
		expected = "<llsd><map><key>foo</key><string>bar</string></map></llsd>\n";
		xml_test("1 element map", expected);

		mSD["baz"] = LLSD();
		expected = "<llsd><map><key>baz</key><undef /><key>foo</key><string>bar</string></map></llsd>\n";
		xml_test("2 element map", expected);
	}
	
	template<> template<>
	void sd_xml_object::test<6>()
	{
		// tests with binary
		std::string expected;

		// Generated by: echo -n 'hello' | openssl enc -e -base64
		mSD = string_to_vector("hello");
		expected = "<llsd><binary encoding=\"base64\">aGVsbG8=</binary></llsd>\n";
		xml_test("binary", expected);

		mSD = string_to_vector("6|6|asdfhappybox|60e44ec5-305c-43c2-9a19-b4b89b1ae2a6|60e44ec5-305c-43c2-9a19-b4b89b1ae2a6|60e44ec5-305c-43c2-9a19-b4b89b1ae2a6|00000000-0000-0000-0000-000000000000|7fffffff|7fffffff|0|0|82000|450fe394-2904-c9ad-214c-a07eb7feec29|(No Description)|0|10|0");
		expected = "<llsd><binary encoding=\"base64\">Nnw2fGFzZGZoYXBweWJveHw2MGU0NGVjNS0zMDVjLTQzYzItOWExOS1iNGI4OWIxYWUyYTZ8NjBlNDRlYzUtMzA1Yy00M2MyLTlhMTktYjRiODliMWFlMmE2fDYwZTQ0ZWM1LTMwNWMtNDNjMi05YTE5LWI0Yjg5YjFhZTJhNnwwMDAwMDAwMC0wMDAwLTAwMDAtMDAwMC0wMDAwMDAwMDAwMDB8N2ZmZmZmZmZ8N2ZmZmZmZmZ8MHwwfDgyMDAwfDQ1MGZlMzk0LTI5MDQtYzlhZC0yMTRjLWEwN2ViN2ZlZWMyOXwoTm8gRGVzY3JpcHRpb24pfDB8MTB8MA==</binary></llsd>\n";
		xml_test("binary", expected);
	}
	
	class TestLLSDSerializeData
	{
	public:
		TestLLSDSerializeData();
		~TestLLSDSerializeData();

		void doRoundTripTests(const std::string&);
		void checkRoundTrip(const std::string&, const LLSD& v);
		
		LLPointer<LLSDFormatter> mFormatter;
		LLPointer<LLSDParser> mParser;
	};

	TestLLSDSerializeData::TestLLSDSerializeData()
	{
	}

	TestLLSDSerializeData::~TestLLSDSerializeData()
	{
	}

	void TestLLSDSerializeData::checkRoundTrip(const std::string& msg, const LLSD& v)
	{
		std::stringstream stream;	
		mFormatter->format(v, stream);
		//llinfos << "checkRoundTrip: length " << stream.str().length() << llendl;
		LLSD w;
		mParser->reset();	// reset() call is needed since test code re-uses mParser
		mParser->parse(stream, w, stream.str().size());
		
		try
		{
			ensure_equals(msg.c_str(), w, v);
		}
		catch (...)
		{
			std::cerr << "the serialized string was:" << std::endl;
			std::cerr << stream.str() << std::endl;
			throw;
		}
	}

	static void fillmap(LLSD& root, U32 width, U32 depth)
	{
		if(depth == 0)
		{
			root["foo"] = "bar";
			return;
		}

		for(U32 i = 0; i < width; ++i)
		{
			std::string key = llformat("child %d", i);
			root[key] = LLSD::emptyMap();
			fillmap(root[key], width, depth - 1);
		}
	}
	
	void TestLLSDSerializeData::doRoundTripTests(const std::string& msg)
	{
		LLSD v;
		checkRoundTrip(msg + " undefined", v);
		
		v = true;
		checkRoundTrip(msg + " true bool", v);
		
		v = false;
		checkRoundTrip(msg + " false bool", v);
		
		v = 1;
		checkRoundTrip(msg + " positive int", v);
		
		v = 0;
		checkRoundTrip(msg + " zero int", v);
		
		v = -1;
		checkRoundTrip(msg + " negative int", v);
		
		v = 1234.5f;
		checkRoundTrip(msg + " positive float", v);
		
		v = 0.0f;
		checkRoundTrip(msg + " zero float", v);
		
		v = -1234.5f;
		checkRoundTrip(msg + " negative float", v);
		
		// FIXME: need a NaN test
		
		v = LLUUID::null;
		checkRoundTrip(msg + " null uuid", v);
		
		LLUUID newUUID;
		newUUID.generate();
		v = newUUID;
		checkRoundTrip(msg + " new uuid", v);
		
		v = "";
		checkRoundTrip(msg + " empty string", v);
		
		v = "some string";
		checkRoundTrip(msg + " non-empty string", v);
		
		v =
"Second Life is a 3-D virtual world entirely built and owned by its residents. "
"Since opening to the public in 2003, it has grown explosively and today is "
"inhabited by nearly 100,000 people from around the globe.\n"
"\n"
"From the moment you enter the World you'll discover a vast digital continent, "
"teeming with people, entertainment, experiences and opportunity. Once you've "
"explored a bit, perhaps you'll find a perfect parcel of land to build your "
"house or business.\n"
"\n"
"You'll also be surrounded by the Creations of your fellow residents. Because "
"residents retain the rights to their digital creations, they can buy, sell "
"and trade with other residents.\n"
"\n"
"The Marketplace currently supports millions of US dollars in monthly "
"transactions. This commerce is handled with the in-world currency, the Linden "
"dollar, which can be converted to US dollars at several thriving online "
"currency exchanges.\n"
"\n"
"Welcome to Second Life. We look forward to seeing you in-world!\n"
		;
		checkRoundTrip(msg + " long string", v);

		static const U32 block_size = 0x000020;
		for (U32 block = 0x000000; block <= 0x10ffff; block += block_size)
		{
			std::ostringstream out;
			
			for (U32 c = block; c < block + block_size; ++c)
			{
				if (c <= 0x000001f
					&& c != 0x000009
					&& c != 0x00000a)
				{
					// see XML standard, sections 2.2 and 4.1
					continue;
				}
				if (0x00d800 <= c  &&  c <= 0x00dfff) { continue; }
				if (0x00fdd0 <= c  &&  c <= 0x00fdef) { continue; }
				if ((c & 0x00fffe) == 0x00fffe) { continue; }		
					// see Unicode standard, section 15.8 
				
				if (c <= 0x00007f)
				{
					out << (char)(c & 0x7f);
				}
				else if (c <= 0x0007ff)
				{
					out << (char)(0xc0 | ((c >> 6) & 0x1f));
					out << (char)(0x80 | ((c >> 0) & 0x3f));
				}
				else if (c <= 0x00ffff)
				{
					out << (char)(0xe0 | ((c >> 12) & 0x0f));
					out << (char)(0x80 | ((c >>  6) & 0x3f));
					out << (char)(0x80 | ((c >>  0) & 0x3f));
				}
				else
				{
					out << (char)(0xf0 | ((c >> 18) & 0x07));
					out << (char)(0x80 | ((c >> 12) & 0x3f));
					out << (char)(0x80 | ((c >>  6) & 0x3f));
					out << (char)(0x80 | ((c >>  0) & 0x3f));
				}
			}
			
			v = out.str();

			std::ostringstream blockmsg;
			blockmsg << msg << " unicode string block 0x" << std::hex << block; 
			checkRoundTrip(blockmsg.str(), v);
		}
		
		LLDate epoch;
		v = epoch;
		checkRoundTrip(msg + " epoch date", v);
		
		LLDate aDay("2002-12-07T05:07:15.00Z");
		v = aDay;
		checkRoundTrip(msg + " date", v);
		
		LLURI path("http://slurl.com/secondlife/Ambleside/57/104/26/");
		v = path;
		checkRoundTrip(msg + " url", v);
		
		const char source[] = "it must be a blue moon again";
		std::vector<U8> data;
		copy(&source[0], &source[sizeof(source)], back_inserter(data));
		
		v = data;
		checkRoundTrip(msg + " binary", v);
		
		v = LLSD::emptyMap();
		checkRoundTrip(msg + " empty map", v);
		
		v = LLSD::emptyMap();
		v["name"] = "luke";		//v.insert("name", "luke");
		v["age"] = 3;			//v.insert("age", 3);
		checkRoundTrip(msg + " map", v);
		
		v.clear();
		v["a"]["1"] = true;
		v["b"]["0"] = false;
		checkRoundTrip(msg + " nested maps", v);
		
		v = LLSD::emptyArray();
		checkRoundTrip(msg + " empty array", v);
		
		v = LLSD::emptyArray();
		v.append("ali");
		v.append(28);
		checkRoundTrip(msg + " array", v);
		
		v.clear();
		v[0][0] = true;
		v[1][0] = false;
		checkRoundTrip(msg + " nested arrays", v);

		v = LLSD::emptyMap();
		fillmap(v, 10, 3); // 10^6 maps
		checkRoundTrip(msg + " many nested maps", v);
	}
	
	typedef tut::test_group<TestLLSDSerializeData> TestLLSDSerialzeGroup;
	typedef TestLLSDSerialzeGroup::object TestLLSDSerializeObject;
	TestLLSDSerialzeGroup gTestLLSDSerializeGroup("llsd serialization");

	template<> template<> 
	void TestLLSDSerializeObject::test<1>()
	{
		mFormatter = new LLSDNotationFormatter();
		mParser = new LLSDNotationParser();
		doRoundTripTests("notation serialization");
	}
	
	template<> template<> 
	void TestLLSDSerializeObject::test<2>()
	{
		mFormatter = new LLSDXMLFormatter();
		mParser = new LLSDXMLParser();
		doRoundTripTests("xml serialization");
	}
	
	template<> template<> 
	void TestLLSDSerializeObject::test<3>()
	{
		mFormatter = new LLSDBinaryFormatter();
		mParser = new LLSDBinaryParser();
		doRoundTripTests("binary serialization");
	}


	/**
	 * @class TestLLSDParsing
	 * @brief Base class for of a parse tester.
	 */
	template <class parser_t>
	class TestLLSDParsing
	{
	public:
		TestLLSDParsing()
		{
			mParser = new parser_t;
		}

		void ensureParse(
			const std::string& msg,
			const std::string& in,
			const LLSD& expected_value,
			S32 expected_count)
		{
			std::stringstream input;
			input.str(in);

			LLSD parsed_result;
			mParser->reset();	// reset() call is needed since test code re-uses mParser
			S32 parsed_count = mParser->parse(input, parsed_result, in.size());
			ensure_equals(msg.c_str(), parsed_result, expected_value);

			// This count check is really only useful for expected
			// parse failures, since the ensures equal will already
			// require eqality.
			std::string count_msg(msg);
			count_msg += " (count)";
			ensure_equals(count_msg, parsed_count, expected_count);
		}

		LLPointer<parser_t> mParser;
	};


	/**
	 * @class TestLLSDXMLParsing
	 * @brief Concrete instance of a parse tester.
	 */
	class TestLLSDXMLParsing : public TestLLSDParsing<LLSDXMLParser>
	{
	public:
		TestLLSDXMLParsing() {}
	};
	
	typedef tut::test_group<TestLLSDXMLParsing> TestLLSDXMLParsingGroup;
	typedef TestLLSDXMLParsingGroup::object TestLLSDXMLParsingObject;
	TestLLSDXMLParsingGroup gTestLLSDXMLParsingGroup("llsd XML parsing");

	template<> template<> 
	void TestLLSDXMLParsingObject::test<1>()
	{
		// test handling of xml not recognized as llsd results in an
		// LLSD Undefined
		ensureParse(
			"malformed xml",
			"<llsd><string>ha ha</string>",
			LLSD(),
			LLSDParser::PARSE_FAILURE);
		ensureParse(
			"not llsd",
			"<html><body><p>ha ha</p></body></html>",
			LLSD(),
			LLSDParser::PARSE_FAILURE);
		ensureParse(
			"value without llsd",
			"<string>ha ha</string>",
			LLSD(),
			LLSDParser::PARSE_FAILURE);
		ensureParse(
			"key without llsd",
			"<key>ha ha</key>",
			LLSD(),
			LLSDParser::PARSE_FAILURE);
	}
	
	
	template<> template<> 
	void TestLLSDXMLParsingObject::test<2>()
	{
		// test handling of unrecognized or unparseable llsd values
		LLSD v;
		v["amy"] = 23;
		v["bob"] = LLSD();
		v["cam"] = 1.23;
		
		ensureParse(
			"unknown data type",
			"<llsd><map>"
				"<key>amy</key><integer>23</integer>"
				"<key>bob</key><bigint>99999999999999999</bigint>"
				"<key>cam</key><real>1.23</real>"
			"</map></llsd>",
			v,
			v.size() + 1);
	}
	
	template<> template<> 
	void TestLLSDXMLParsingObject::test<3>()
	{
		// test handling of nested bad data
		
		LLSD v;
		v["amy"] = 23;
		v["cam"] = 1.23;
		
		ensureParse(
			"map with html",
			"<llsd><map>"
				"<key>amy</key><integer>23</integer>"
				"<html><body>ha ha</body></html>"
				"<key>cam</key><real>1.23</real>"
			"</map></llsd>",
			v,
			v.size() + 1);
			
		v.clear();
		v["amy"] = 23;
		v["cam"] = 1.23;
		ensureParse(
			"map with value for key",
			"<llsd><map>"
				"<key>amy</key><integer>23</integer>"
				"<string>ha ha</string>"
				"<key>cam</key><real>1.23</real>"
			"</map></llsd>",
			v,
			v.size() + 1);
			
		v.clear();
		v["amy"] = 23;
		v["bob"] = LLSD::emptyMap();
		v["cam"] = 1.23;
		ensureParse(
			"map with map of html",
			"<llsd><map>"
				"<key>amy</key><integer>23</integer>"
				"<key>bob</key>"
				"<map>"
					"<html><body>ha ha</body></html>"
				"</map>"
				"<key>cam</key><real>1.23</real>"
			"</map></llsd>",
			v,
			v.size() + 1);

		v.clear();
		v[0] = 23;
		v[1] = LLSD();
		v[2] = 1.23;
		
		ensureParse(
			"array value of html",
			"<llsd><array>"
				"<integer>23</integer>"
				"<html><body>ha ha</body></html>"
				"<real>1.23</real>"
			"</array></llsd>",
			v,
			v.size() + 1);
			
		v.clear();
		v[0] = 23;
		v[1] = LLSD::emptyMap();
		v[2] = 1.23;
		ensureParse(
			"array with map of html",
			"<llsd><array>"
				"<integer>23</integer>"
				"<map>"
					"<html><body>ha ha</body></html>"
				"</map>"
				"<real>1.23</real>"
			"</array></llsd>",
			v,
			v.size() + 1);
	}

	template<> template<> 
	void TestLLSDXMLParsingObject::test<4>()
	{
		// test handling of binary object in XML
		std::string xml;
		LLSD expected;

		// Generated by: echo -n 'hello' | openssl enc -e -base64
		expected = string_to_vector("hello");
		xml = "<llsd><binary encoding=\"base64\">aGVsbG8=</binary></llsd>\n";
		ensureParse(
			"the word 'hello' packed in binary encoded base64",
			xml,
			expected,
			1);

		expected = string_to_vector("6|6|asdfhappybox|60e44ec5-305c-43c2-9a19-b4b89b1ae2a6|60e44ec5-305c-43c2-9a19-b4b89b1ae2a6|60e44ec5-305c-43c2-9a19-b4b89b1ae2a6|00000000-0000-0000-0000-000000000000|7fffffff|7fffffff|0|0|82000|450fe394-2904-c9ad-214c-a07eb7feec29|(No Description)|0|10|0");
		xml = "<llsd><binary encoding=\"base64\">Nnw2fGFzZGZoYXBweWJveHw2MGU0NGVjNS0zMDVjLTQzYzItOWExOS1iNGI4OWIxYWUyYTZ8NjBlNDRlYzUtMzA1Yy00M2MyLTlhMTktYjRiODliMWFlMmE2fDYwZTQ0ZWM1LTMwNWMtNDNjMi05YTE5LWI0Yjg5YjFhZTJhNnwwMDAwMDAwMC0wMDAwLTAwMDAtMDAwMC0wMDAwMDAwMDAwMDB8N2ZmZmZmZmZ8N2ZmZmZmZmZ8MHwwfDgyMDAwfDQ1MGZlMzk0LTI5MDQtYzlhZC0yMTRjLWEwN2ViN2ZlZWMyOXwoTm8gRGVzY3JpcHRpb24pfDB8MTB8MA==</binary></llsd>\n";
		ensureParse(
			"a common binary blob for object -> agent offline inv transfer",
			xml,
			expected,
			1);

		expected = string_to_vector("6|6|asdfhappybox|60e44ec5-305c-43c2-9a19-b4b89b1ae2a6|60e44ec5-305c-43c2-9a19-b4b89b1ae2a6|60e44ec5-305c-43c2-9a19-b4b89b1ae2a6|00000000-0000-0000-0000-000000000000|7fffffff|7fffffff|0|0|82000|450fe394-2904-c9ad-214c-a07eb7feec29|(No Description)|0|10|0");
		xml = "<llsd><binary encoding=\"base64\">Nnw2fGFzZGZoYXBweWJveHw2MGU0NGVjNS0zMDVjLTQzYzItOWExOS1iNGI4OWIxYWUyYTZ8NjBl\n";
		xml += "NDRlYzUtMzA1Yy00M2MyLTlhMTktYjRiODliMWFlMmE2fDYwZTQ0ZWM1LTMwNWMtNDNjMi05YTE5\n";
		xml += "LWI0Yjg5YjFhZTJhNnwwMDAwMDAwMC0wMDAwLTAwMDAtMDAwMC0wMDAwMDAwMDAwMDB8N2ZmZmZm\n";
		xml += "ZmZ8N2ZmZmZmZmZ8MHwwfDgyMDAwfDQ1MGZlMzk0LTI5MDQtYzlhZC0yMTRjLWEwN2ViN2ZlZWMy\n";
		xml += "OXwoTm8gRGVzY3JpcHRpb24pfDB8MTB8MA==</binary></llsd>\n";
		ensureParse(
			"a common binary blob for object -> agent offline inv transfer",
			xml,
			expected,
			1);
	}
	/*
	TODO:
		test XML parsing
			binary with unrecognized encoding
			nested LLSD tags
			multiple values inside an LLSD
	*/


	/**
	 * @class TestLLSDNotationParsing
	 * @brief Concrete instance of a parse tester.
	 */
	class TestLLSDNotationParsing : public TestLLSDParsing<LLSDNotationParser>
	{
	public:
		TestLLSDNotationParsing() {}
	};

	typedef tut::test_group<TestLLSDNotationParsing> TestLLSDNotationParsingGroup;
	typedef TestLLSDNotationParsingGroup::object TestLLSDNotationParsingObject;
	TestLLSDNotationParsingGroup gTestLLSDNotationParsingGroup(
		"llsd notation parsing");

	template<> template<> 
	void TestLLSDNotationParsingObject::test<1>()
	{
		// test handling of xml not recognized as llsd results in an
		// LLSD Undefined
		ensureParse(
			"malformed notation map",
			"{'ha ha'",
			LLSD(),
			LLSDParser::PARSE_FAILURE);
		ensureParse(
			"malformed notation array",
			"['ha ha'",
			LLSD(),
			LLSDParser::PARSE_FAILURE);
		ensureParse(
			"malformed notation string",
			"'ha ha",
			LLSD(),
			LLSDParser::PARSE_FAILURE);
		ensureParse(
			"bad notation noise",
			"g48ejlnfr",
			LLSD(),
			LLSDParser::PARSE_FAILURE);
	}

	template<> template<> 
	void TestLLSDNotationParsingObject::test<2>()
	{
		ensureParse("valid undef", "!", LLSD(), 1);
	}

	template<> template<> 
	void TestLLSDNotationParsingObject::test<3>()
	{
		LLSD val = false;
		ensureParse("valid boolean false 0", "false", val, 1);
		ensureParse("valid boolean false 1", "f", val, 1);
		ensureParse("valid boolean false 2", "0", val, 1);
		ensureParse("valid boolean false 3", "F", val, 1);
		ensureParse("valid boolean false 4", "FALSE", val, 1);
		val = true;
		ensureParse("valid boolean true 0", "true", val, 1);
		ensureParse("valid boolean true 1", "t", val, 1);
		ensureParse("valid boolean true 2", "1", val, 1);
		ensureParse("valid boolean true 3", "T", val, 1);
		ensureParse("valid boolean true 4", "TRUE", val, 1);

		val.clear();
		ensureParse("invalid true", "TR", val, LLSDParser::PARSE_FAILURE);
		ensureParse("invalid false", "FAL", val, LLSDParser::PARSE_FAILURE);
	}

	template<> template<> 
	void TestLLSDNotationParsingObject::test<4>()
	{
		LLSD val = 123;
		ensureParse("valid integer", "i123", val, 1);
		val.clear();
		ensureParse("invalid integer", "421", val, LLSDParser::PARSE_FAILURE);
	}

	template<> template<> 
	void TestLLSDNotationParsingObject::test<5>()
	{
		LLSD val = 456.7;
		ensureParse("valid real", "r456.7", val, 1);
		val.clear();
		ensureParse("invalid real", "456.7", val, LLSDParser::PARSE_FAILURE);
	}

	template<> template<> 
	void TestLLSDNotationParsingObject::test<6>()
	{
		LLUUID id;
		LLSD val = id;
		ensureParse(
			"unparseable uuid",
			"u123",
			LLSD(),
			LLSDParser::PARSE_FAILURE);
		id.generate();
		val = id;
		std::string uuid_str("u");
		uuid_str += id.asString();
		ensureParse("valid uuid", uuid_str.c_str(), val, 1);
	}

	template<> template<> 
	void TestLLSDNotationParsingObject::test<7>()
	{
		LLSD val = std::string("foolish");
		ensureParse("valid string 1", "\"foolish\"", val, 1);
		val = std::string("g'day");
		ensureParse("valid string 2", "\"g'day\"", val, 1);
		val = std::string("have a \"nice\" day");
		ensureParse("valid string 3", "'have a \"nice\" day'", val, 1);
		val = std::string("whatever");
		ensureParse("valid string 4", "s(8)\"whatever\"", val, 1);
	}

	template<> template<> 
	void TestLLSDNotationParsingObject::test<8>()
	{
		ensureParse(
			"invalid string 1",
			"s(7)\"whatever\"",
			LLSD(),
			LLSDParser::PARSE_FAILURE);
		ensureParse(
			"invalid string 2",
			"s(9)\"whatever\"",
			LLSD(),
			LLSDParser::PARSE_FAILURE);
	}

	template<> template<> 
	void TestLLSDNotationParsingObject::test<9>()
	{
		LLSD val = LLURI("http://www.google.com");
		ensureParse("valid uri", "l\"http://www.google.com\"", val, 1);
	}

	template<> template<> 
	void TestLLSDNotationParsingObject::test<10>()
	{
		LLSD val = LLDate("2007-12-28T09:22:53.10Z");
		ensureParse("valid date", "d\"2007-12-28T09:22:53.10Z\"", val, 1);
	}

	template<> template<> 
	void TestLLSDNotationParsingObject::test<11>()
	{
		std::vector<U8> vec;
		vec.push_back((U8)'a'); vec.push_back((U8)'b'); vec.push_back((U8)'c');
		vec.push_back((U8)'3'); vec.push_back((U8)'2'); vec.push_back((U8)'1');
		LLSD val = vec;
		ensureParse("valid binary b64", "b64\"YWJjMzIx\"", val, 1);
		ensureParse("valid bainry b16", "b16\"616263333231\"", val, 1);
		ensureParse("valid bainry raw", "b(6)\"abc321\"", val, 1);
	}

	template<> template<> 
	void TestLLSDNotationParsingObject::test<12>()
	{
		ensureParse(
			"invalid -- binary length specified too long",
			"b(7)\"abc321\"",
			LLSD(),
			LLSDParser::PARSE_FAILURE);
		ensureParse(
			"invalid -- binary length specified way too long",
			"b(1000000)\"abc321\"",
			LLSD(),
			LLSDParser::PARSE_FAILURE);
	}

	template<> template<> 
	void TestLLSDNotationParsingObject::test<13>()
	{
		LLSD val;
		val["amy"] = 23;
		val["bob"] = LLSD();
		val["cam"] = 1.23;
		ensureParse("simple map", "{'amy':i23,'bob':!,'cam':r1.23}", val, 4);

		val["bob"] = LLSD::emptyMap();
		val["bob"]["vehicle"] = std::string("bicycle");
		ensureParse(
			"nested map",
			"{'amy':i23,'bob':{'vehicle':'bicycle'},'cam':r1.23}",
			val,
			5);
	}

	template<> template<> 
	void TestLLSDNotationParsingObject::test<14>()
	{
		LLSD val;
		val.append(23);
		val.append(LLSD());
		val.append(1.23);
		ensureParse("simple array", "[i23,!,r1.23]", val, 4);
		val[1] = LLSD::emptyArray();
		val[1].append("bicycle");
		ensureParse("nested array", "[i23,['bicycle'],r1.23]", val, 5);
	}

	template<> template<> 
	void TestLLSDNotationParsingObject::test<15>()
	{
		LLSD val;
		val["amy"] = 23;
		val["bob"]["dogs"] = LLSD::emptyArray();
		val["bob"]["dogs"].append(LLSD::emptyMap());
		val["bob"]["dogs"][0]["name"] = std::string("groove");
		val["bob"]["dogs"][0]["breed"] = std::string("samoyed");
		val["bob"]["dogs"].append(LLSD::emptyMap());
		val["bob"]["dogs"][1]["name"] = std::string("greyley");
		val["bob"]["dogs"][1]["breed"] = std::string("chow/husky");
		val["cam"] = 1.23;
		ensureParse(
			"nested notation",
			"{'amy':i23,"
			" 'bob':{'dogs':["
			         "{'name':'groove', 'breed':'samoyed'},"
			         "{'name':'greyley', 'breed':'chow/husky'}]},"
			" 'cam':r1.23}",
			val,
			11);
	}

	template<> template<> 
	void TestLLSDNotationParsingObject::test<16>()
	{
		// text to make sure that incorrect sizes bail because 
		std::string bad_str("s(5)\"hi\"");
		ensureParse(
			"size longer than bytes left",
			bad_str,
			LLSD(),
			LLSDParser::PARSE_FAILURE);
	}

	template<> template<> 
	void TestLLSDNotationParsingObject::test<17>()
	{
		// text to make sure that incorrect sizes bail because 
		std::string bad_bin("b(5)\"hi\"");
		ensureParse(
			"size longer than bytes left",
			bad_bin,
			LLSD(),
			LLSDParser::PARSE_FAILURE);
	}

	/**
	 * @class TestLLSDBinaryParsing
	 * @brief Concrete instance of a parse tester.
	 */
	class TestLLSDBinaryParsing : public TestLLSDParsing<LLSDBinaryParser>
	{
	public:
		TestLLSDBinaryParsing() {}
	};

	typedef tut::test_group<TestLLSDBinaryParsing> TestLLSDBinaryParsingGroup;
	typedef TestLLSDBinaryParsingGroup::object TestLLSDBinaryParsingObject;
	TestLLSDBinaryParsingGroup gTestLLSDBinaryParsingGroup(
		"llsd binary parsing");

	template<> template<> 
	void TestLLSDBinaryParsingObject::test<1>()
	{
		std::vector<U8> vec;
		vec.resize(6);
		vec[0] = 'a'; vec[1] = 'b'; vec[2] = 'c';
		vec[3] = '3'; vec[4] = '2'; vec[5] = '1';
		std::string string_expected((char*)&vec[0], vec.size());
		LLSD value = string_expected;

		vec.resize(11);
		vec[0] = 's'; // for string
		vec[5] = 'a'; vec[6] = 'b'; vec[7] = 'c';
		vec[8] = '3'; vec[9] = '2'; vec[10] = '1';

		uint32_t size = htonl(6);
		memcpy(&vec[1], &size, sizeof(uint32_t));
		std::string str_good((char*)&vec[0], vec.size());
		ensureParse("correct string parse", str_good, value, 1);

		size = htonl(7);
		memcpy(&vec[1], &size, sizeof(uint32_t));
		std::string str_bad_1((char*)&vec[0], vec.size());
		ensureParse(
			"incorrect size string parse",
			str_bad_1,
			LLSD(),
			LLSDParser::PARSE_FAILURE);

		size = htonl(100000);
		memcpy(&vec[1], &size, sizeof(uint32_t));
		std::string str_bad_2((char*)&vec[0], vec.size());
		ensureParse(
			"incorrect size string parse",
			str_bad_2,
			LLSD(),
			LLSDParser::PARSE_FAILURE);
	}

	template<> template<> 
	void TestLLSDBinaryParsingObject::test<2>()
	{
		std::vector<U8> vec;
		vec.resize(6);
		vec[0] = 'a'; vec[1] = 'b'; vec[2] = 'c';
		vec[3] = '3'; vec[4] = '2'; vec[5] = '1';
		LLSD value = vec;
		
		vec.resize(11);
		vec[0] = 'b';  // for binary
		vec[5] = 'a'; vec[6] = 'b'; vec[7] = 'c';
		vec[8] = '3'; vec[9] = '2'; vec[10] = '1';

		uint32_t size = htonl(6);
		memcpy(&vec[1], &size, sizeof(uint32_t));
		std::string str_good((char*)&vec[0], vec.size());
		ensureParse("correct binary parse", str_good, value, 1);

		size = htonl(7);
		memcpy(&vec[1], &size, sizeof(uint32_t));
		std::string str_bad_1((char*)&vec[0], vec.size());
		ensureParse(
			"incorrect size binary parse 1",
			str_bad_1,
			LLSD(),
			LLSDParser::PARSE_FAILURE);

		size = htonl(100000);
		memcpy(&vec[1], &size, sizeof(uint32_t));
		std::string str_bad_2((char*)&vec[0], vec.size());
		ensureParse(
			"incorrect size binary parse 2",
			str_bad_2,
			LLSD(),
			LLSDParser::PARSE_FAILURE);
	}

	template<> template<> 
	void TestLLSDBinaryParsingObject::test<3>()
	{
		// test handling of xml not recognized as llsd results in an
		// LLSD Undefined
		ensureParse(
			"malformed binary map",
			"{'ha ha'",
			LLSD(),
			LLSDParser::PARSE_FAILURE);
		ensureParse(
			"malformed binary array",
			"['ha ha'",
			LLSD(),
			LLSDParser::PARSE_FAILURE);
		ensureParse(
			"malformed binary string",
			"'ha ha",
			LLSD(),
			LLSDParser::PARSE_FAILURE);
		ensureParse(
			"bad noise",
			"g48ejlnfr",
			LLSD(),
			LLSDParser::PARSE_FAILURE);
	}
	template<> template<> 
	void TestLLSDBinaryParsingObject::test<4>()
	{
		ensureParse("valid undef", "!", LLSD(), 1);
	}

	template<> template<> 
	void TestLLSDBinaryParsingObject::test<5>()
	{
		LLSD val = false;
		ensureParse("valid boolean false 2", "0", val, 1);
		val = true;
		ensureParse("valid boolean true 2", "1", val, 1);

		val.clear();
		ensureParse("invalid true", "t", val, LLSDParser::PARSE_FAILURE);
		ensureParse("invalid false", "f", val, LLSDParser::PARSE_FAILURE);
	}

	template<> template<> 
	void TestLLSDBinaryParsingObject::test<6>()
	{
		std::vector<U8> vec;
		vec.push_back('{');
		vec.resize(vec.size() + 4);
		uint32_t size = htonl(1);
		memcpy(&vec[1], &size, sizeof(uint32_t));
		vec.push_back('k');
		int key_size_loc = vec.size();
		size = htonl(1); // 1 too short
		vec.resize(vec.size() + 4);
		memcpy(&vec[key_size_loc], &size, sizeof(uint32_t));
		vec.push_back('a'); vec.push_back('m'); vec.push_back('y');
		vec.push_back('i');
		int integer_loc = vec.size();
		vec.resize(vec.size() + 4);
		uint32_t val_int = htonl(23);
		memcpy(&vec[integer_loc], &val_int, sizeof(uint32_t));
		std::string str_bad_1((char*)&vec[0], vec.size());
		ensureParse(
			"invalid key size",
			str_bad_1,
			LLSD(),
			LLSDParser::PARSE_FAILURE);

		// check with correct size, but unterminated map (missing '}')
		size = htonl(3); // correct size
		memcpy(&vec[key_size_loc], &size, sizeof(uint32_t));
		std::string str_bad_2((char*)&vec[0], vec.size());
		ensureParse(
			"valid key size, unterminated map",
			str_bad_2,
			LLSD(),
			LLSDParser::PARSE_FAILURE);

		// check w/ correct size and correct map termination
		LLSD val;
		val["amy"] = 23;
		vec.push_back('}');
		std::string str_good((char*)&vec[0], vec.size());
		ensureParse(
			"valid map",
			str_good,
			val,
			2);

		// check w/ incorrect sizes and correct map termination
		size = htonl(0); // 1 too few (for the map entry)
		memcpy(&vec[1], &size, sizeof(uint32_t));
		std::string str_bad_3((char*)&vec[0], vec.size());
		ensureParse(
			"invalid map too long",
			str_bad_3,
			LLSD(),
			LLSDParser::PARSE_FAILURE);

		size = htonl(2); // 1 too many
		memcpy(&vec[1], &size, sizeof(uint32_t));
		std::string str_bad_4((char*)&vec[0], vec.size());
		ensureParse(
			"invalid map too short",
			str_bad_4,
			LLSD(),
			LLSDParser::PARSE_FAILURE);
	}

	template<> template<> 
	void TestLLSDBinaryParsingObject::test<7>()
	{
		std::vector<U8> vec;
		vec.push_back('[');
		vec.resize(vec.size() + 4);
		uint32_t size = htonl(1); // 1 too short
		memcpy(&vec[1], &size, sizeof(uint32_t));
		vec.push_back('"'); vec.push_back('a'); vec.push_back('m');
		vec.push_back('y'); vec.push_back('"'); vec.push_back('i');
		int integer_loc = vec.size();
		vec.resize(vec.size() + 4);
		uint32_t val_int = htonl(23);
		memcpy(&vec[integer_loc], &val_int, sizeof(uint32_t));

		std::string str_bad_1((char*)&vec[0], vec.size());
		ensureParse(
			"invalid array size",
			str_bad_1,
			LLSD(),
			LLSDParser::PARSE_FAILURE);

		// check with correct size, but unterminated map (missing ']')
		size = htonl(2); // correct size
		memcpy(&vec[1], &size, sizeof(uint32_t));
		std::string str_bad_2((char*)&vec[0], vec.size());
		ensureParse(
			"unterminated array",
			str_bad_2,
			LLSD(),
			LLSDParser::PARSE_FAILURE);

		// check w/ correct size and correct map termination
		LLSD val;
		val.append("amy");
		val.append(23);
		vec.push_back(']');
		std::string str_good((char*)&vec[0], vec.size());
		ensureParse(
			"valid array",
			str_good,
			val,
			3);

		// check with too many elements
		size = htonl(3); // 1 too long
		memcpy(&vec[1], &size, sizeof(uint32_t));
		std::string str_bad_3((char*)&vec[0], vec.size());
		ensureParse(
			"array too short",
			str_bad_3,
			LLSD(),
			LLSDParser::PARSE_FAILURE);
	}

	template<> template<> 
	void TestLLSDBinaryParsingObject::test<8>()
	{
		std::vector<U8> vec;
		vec.push_back('{');
		vec.resize(vec.size() + 4);
		memset(&vec[1], 0, 4);
		vec.push_back('}');
		std::string str_good((char*)&vec[0], vec.size());
		LLSD val = LLSD::emptyMap();
		ensureParse(
			"empty map",
			str_good,
			val,
			1);
	}

	template<> template<> 
	void TestLLSDBinaryParsingObject::test<9>()
	{
		std::vector<U8> vec;
		vec.push_back('[');
		vec.resize(vec.size() + 4);
		memset(&vec[1], 0, 4);
		vec.push_back(']');
		std::string str_good((char*)&vec[0], vec.size());
		LLSD val = LLSD::emptyArray();
		ensureParse(
			"empty array",
			str_good,
			val,
			1);
	}

	template<> template<> 
	void TestLLSDBinaryParsingObject::test<10>()
	{
		std::vector<U8> vec;
		vec.push_back('l');
		vec.resize(vec.size() + 4);
		uint32_t size = htonl(14); // 1 too long
		memcpy(&vec[1], &size, sizeof(uint32_t));
		vec.push_back('h'); vec.push_back('t'); vec.push_back('t');
		vec.push_back('p'); vec.push_back(':'); vec.push_back('/');
		vec.push_back('/'); vec.push_back('s'); vec.push_back('l');
		vec.push_back('.'); vec.push_back('c'); vec.push_back('o');
		vec.push_back('m');
		std::string str_bad((char*)&vec[0], vec.size());
		ensureParse(
			"invalid uri length size",
			str_bad,
			LLSD(),
			LLSDParser::PARSE_FAILURE);

		LLSD val;
		val = LLURI("http://sl.com");
		size = htonl(13); // correct length
		memcpy(&vec[1], &size, sizeof(uint32_t));
		std::string str_good((char*)&vec[0], vec.size());
		ensureParse(
			"valid key size",
			str_good,
			val,
			1);
	}

/*
	template<> template<> 
	void TestLLSDBinaryParsingObject::test<11>()
	{
	}
*/

   /**
	 * @class TestLLSDCrossCompatible
	 * @brief Miscellaneous serialization and parsing tests
	 */
	class TestLLSDCrossCompatible
	{
	public:
		TestLLSDCrossCompatible() {}

		void ensureBinaryAndNotation(
			const std::string& msg,
			const LLSD& input)
		{
			// to binary, and back again
			std::stringstream str1;
			S32 count1 = LLSDSerialize::toBinary(input, str1);
			LLSD actual_value_bin;
			S32 count2 = LLSDSerialize::fromBinary(
				actual_value_bin,
				str1,
				LLSDSerialize::SIZE_UNLIMITED);
			ensure_equals(
				"ensureBinaryAndNotation binary count",
				count2,
				count1);

			// to notation and back again
			std::stringstream str2;
			S32 count3 = LLSDSerialize::toNotation(actual_value_bin, str2);
			ensure_equals(
				"ensureBinaryAndNotation notation count1",
				count3,
				count2);
			LLSD actual_value_notation;
			S32 count4 = LLSDSerialize::fromNotation(
				actual_value_notation,
				str2,
				LLSDSerialize::SIZE_UNLIMITED);
			ensure_equals(
				"ensureBinaryAndNotation notation count2",
				count4,
				count3);
			ensure_equals(
				(msg + " (binaryandnotation)").c_str(),
				actual_value_notation,
				input);
		}

		void ensureBinaryAndXML(
			const std::string& msg,
			const LLSD& input)
		{
			// to binary, and back again
			std::stringstream str1;
			S32 count1 = LLSDSerialize::toBinary(input, str1);
			LLSD actual_value_bin;
			S32 count2 = LLSDSerialize::fromBinary(
				actual_value_bin,
				str1,
				LLSDSerialize::SIZE_UNLIMITED);
			ensure_equals(
				"ensureBinaryAndXML binary count",
				count2,
				count1);

			// to xml and back again
			std::stringstream str2;
			S32 count3 = LLSDSerialize::toXML(actual_value_bin, str2);
			ensure_equals(
				"ensureBinaryAndXML xml count1",
				count3,
				count2);
			LLSD actual_value_xml;
			S32 count4 = LLSDSerialize::fromXML(actual_value_xml, str2);
			ensure_equals(
				"ensureBinaryAndXML xml count2",
				count4,
				count3);
			ensure_equals((msg + " (binaryandxml)").c_str(), actual_value_xml, input);
		}
	};

	typedef tut::test_group<TestLLSDCrossCompatible> TestLLSDCompatibleGroup;
	typedef TestLLSDCompatibleGroup::object TestLLSDCompatibleObject;
	TestLLSDCompatibleGroup gTestLLSDCompatibleGroup(
		"llsd serialize compatible");

	template<> template<> 
	void TestLLSDCompatibleObject::test<1>()
	{
		LLSD test;
		ensureBinaryAndNotation("undef", test);
		ensureBinaryAndXML("undef", test);
		test = true;
		ensureBinaryAndNotation("boolean true", test);
		ensureBinaryAndXML("boolean true", test);
		test = false;
		ensureBinaryAndNotation("boolean false", test);
		ensureBinaryAndXML("boolean false", test);
		test = 0;
		ensureBinaryAndNotation("integer zero", test);
		ensureBinaryAndXML("integer zero", test);
		test = 1;
		ensureBinaryAndNotation("integer positive", test);
		ensureBinaryAndXML("integer positive", test);
		test = -234567;
		ensureBinaryAndNotation("integer negative", test);
		ensureBinaryAndXML("integer negative", test);
		test = 0.0;
		ensureBinaryAndNotation("real zero", test);
		ensureBinaryAndXML("real zero", test);
		test = 1.0;
		ensureBinaryAndNotation("real positive", test);
		ensureBinaryAndXML("real positive", test);
		test = -1.0;
		ensureBinaryAndNotation("real negative", test);
		ensureBinaryAndXML("real negative", test);
	}

	template<> template<> 
	void TestLLSDCompatibleObject::test<2>()
	{
		LLSD test;
		test = "foobar";
		ensureBinaryAndNotation("string", test);
		ensureBinaryAndXML("string", test);
	}

	template<> template<> 
	void TestLLSDCompatibleObject::test<3>()
	{
		LLSD test;
		LLUUID id;
		id.generate();
		test = id;
		ensureBinaryAndNotation("uuid", test);
		ensureBinaryAndXML("uuid", test);
	}

	template<> template<> 
	void TestLLSDCompatibleObject::test<4>()
	{
		LLSD test;
		test = LLDate(12345.0);
		ensureBinaryAndNotation("date", test);
		ensureBinaryAndXML("date", test);
	}

	template<> template<> 
	void TestLLSDCompatibleObject::test<5>()
	{
		LLSD test;
		test = LLURI("http://www.secondlife.com/");
		ensureBinaryAndNotation("uri", test);
		ensureBinaryAndXML("uri", test);
	}

	template<> template<> 
	void TestLLSDCompatibleObject::test<6>()
	{
		LLSD test;
		typedef std::vector<U8> buf_t;
		buf_t val;
		for(int ii = 0; ii < 100; ++ii)
		{
			srand(ii);		/* Flawfinder: ignore */
			S32 size = rand() % 100 + 10;
			std::generate_n(
				std::back_insert_iterator<buf_t>(val),
				size,
				rand);
		}
		test = val;
		ensureBinaryAndNotation("binary", test);
		ensureBinaryAndXML("binary", test);
	}

	template<> template<> 
	void TestLLSDCompatibleObject::test<7>()
	{
		LLSD test;
		test = LLSD::emptyArray();
		test.append(1);
		test.append("hello");
		ensureBinaryAndNotation("array", test);
		ensureBinaryAndXML("array", test);
	}

	template<> template<> 
	void TestLLSDCompatibleObject::test<8>()
	{
		LLSD test;
		test = LLSD::emptyArray();
		test["foo"] = "bar";
		test["baz"] = 100;
		ensureBinaryAndNotation("map", test);
		ensureBinaryAndXML("map", test);
	}

    struct TestPythonCompatible
    {
        TestPythonCompatible() {}
        ~TestPythonCompatible() {}

        void python(const std::string& desc, const std::string& script /*, F32 timeout=5 */)
        {
            const char* PYTHON(getenv("PYTHON"));
            ensure("Set $PYTHON to the Python interpreter", PYTHON);

            NamedTempScript scriptfile(script);

#if LL_WINDOWS
            std::string q("\"");
            std::string qPYTHON(q + PYTHON + q);
            std::string qscript(q + scriptfile.getName() + q);
            int rc(_spawnl(_P_WAIT, PYTHON, qPYTHON.c_str(), qscript.c_str(), NULL));
            if (rc == -1)
            {
                char buffer[256];
                strerror_s(buffer, errno); // C++ can infer the buffer size!  :-O
                ensure(STRINGIZE("Couldn't run Python " << desc << "script: " << buffer), false);
            }
            else
            {
                ensure_equals(STRINGIZE(desc << " script terminated with rc " << rc), rc, 0);
            }

#else  // LL_DARWIN, LL_LINUX
            LLProcessLauncher py;
            py.setExecutable(PYTHON);
            py.addArgument(scriptfile.getName());
            ensure_equals(STRINGIZE("Couldn't launch " << desc << " script"), py.launch(), 0);
            // Implementing timeout would mean messing with alarm() and
            // catching SIGALRM... later maybe...
            int status(0);
            if (waitpid(py.getProcessID(), &status, 0) == -1)
            {
                int waitpid_errno(errno);
                ensure_equals(STRINGIZE("Couldn't retrieve rc from " << desc << " script: "
                                        "waitpid() errno " << waitpid_errno),
                              waitpid_errno, ECHILD);
            }
            else
            {
                if (WIFEXITED(status))
                {
                    int rc(WEXITSTATUS(status));
                    ensure_equals(STRINGIZE(desc << " script terminated with rc " << rc), rc, 0);
                }
                else if (WIFSIGNALED(status))
                {
                    ensure(STRINGIZE(desc << " script terminated by signal " << WTERMSIG(status)),
                           false);
                }
                else
                {
                    ensure(STRINGIZE(desc << " script produced impossible status " << status),
                           false);
                }
            }
#endif
        }
    };

    typedef tut::test_group<TestPythonCompatible> TestPythonCompatibleGroup;
    typedef TestPythonCompatibleGroup::object TestPythonCompatibleObject;
    TestPythonCompatibleGroup pycompat("LLSD serialize Python compatibility");

    template<> template<>
    void TestPythonCompatibleObject::test<1>()
    {
        python("hello", "print 'Hello, world!'");
    }

    template<> template<>
    void TestPythonCompatibleObject::test<2>()
    {
        python("platform",
"import sys\n"
"print 'Running on', sys.platform");
    }
}
