//
// Created by QY on 2016-12-30.
//

#ifndef SERIALIZE_JSON_HPP
#define SERIALIZE_JSON_HPP
#include <string.h>
#include "reflection.hpp"
#include "traits.hpp"
#include "itoa.hpp"

namespace iguana {
	template <typename alloc_ty>
	struct basic_string_stream
	{
	private:
		alloc_ty alloc;
	public:
		enum { good, read_overflow };

		char * m_header_ptr;
		char * m_read_ptr;
		char * m_write_ptr;
		char * m_tail_ptr;
		int							m_status;
		std::size_t			m_length;

		enum { INIT_BUFF_SIZE = 1024 };
		basic_string_stream() :m_length(INIT_BUFF_SIZE), m_status(good)
		{
			this->m_header_ptr = this->alloc.allocate(INIT_BUFF_SIZE);
			this->m_read_ptr = this->m_header_ptr;
			this->m_write_ptr = this->m_header_ptr;
			this->m_tail_ptr = this->m_header_ptr + m_length;
		}

		~basic_string_stream()
		{
			this->alloc.deallocate(m_header_ptr, this->m_length);
		}

		inline std::size_t write(const char * buffer)
		{
			return write(buffer, strlen(buffer));
		}

		inline std::size_t read(const char * buffer, std::size_t len)
		{
			if (this->m_read_ptr + len > this->m_tail_ptr)
			{
				m_status = read_overflow;
				return 0;
			}
			std::memcpy(buffer, this->m_read_ptr, len);
			this->m_read_ptr += len;
			return len;
		}

		inline std::size_t growpup(std::size_t want_size)
		{
			std::size_t new_size = ((want_size + INIT_BUFF_SIZE - 1) / INIT_BUFF_SIZE)*INIT_BUFF_SIZE;
			std::size_t write_pos = this->m_write_ptr - this->m_header_ptr;
			std::size_t read_pos = this->m_read_ptr - this->m_header_ptr;
			char * temp = this->m_header_ptr;
			this->m_header_ptr = this->alloc.allocate(new_size);
			std::memcpy(this->m_header_ptr, temp, this->m_length);
			this->alloc.deallocate(temp, this->m_length);
			this->m_length = new_size;
			this->m_write_ptr = this->m_header_ptr + write_pos;
			this->m_read_ptr = this->m_header_ptr + read_pos;
			this->m_tail_ptr = this->m_header_ptr + m_length;
			return new_size;
		}

		inline std::size_t write(const char * buffer, std::size_t len)
		{
			std::size_t writed_len = this->m_write_ptr + len - this->m_header_ptr;
			if (writed_len > this->m_length)
			{
				this->growpup(writed_len);
			}
			std::memcpy((void*)this->m_write_ptr, buffer, len);
			this->m_write_ptr += len;
			return len;
		}

		inline void put(char c)
		{
			std::size_t writed_len = this->m_write_ptr + 1 - this->m_header_ptr;
			if (writed_len > this->m_length)
			{
				this->growpup(writed_len);
			}
			*this->m_write_ptr = c;
			++this->m_write_ptr;
		}

		inline bool bad()const { return m_status != good; }

		inline void clear()
		{
			this->m_read_ptr = this->m_header_ptr;
			this->m_write_ptr = this->m_header_ptr;
		}

		inline const char * data() const
		{
			return this->m_header_ptr;
		}

		std::basic_string<char, std::char_traits<char>, alloc_ty> str()
		{
			std::basic_string<char, std::char_traits<char>, alloc_ty> s(this->m_header_ptr, this->write_length());
			return s;
		}

		inline ::std::size_t read_length() const
		{
			return this->m_read_ptr - this->m_header_ptr;
		}

		inline ::std::size_t write_length() const
		{
			return this->m_write_ptr - this->m_header_ptr;
		}
	};

	typedef basic_string_stream<std::allocator<char> > string_stream;

	namespace json {
		template<typename InputIt, typename T, typename F>
		T join(InputIt first, InputIt last, const T &delim, const F &f) {
			T t = f(*first++);
			while (first != last) {
				t += delim;
				t += f(*first++);
			}
			return t;
		}

		template<typename Stream, typename InputIt, typename T, typename F>
		void join(Stream& ss, InputIt first, InputIt last, const T &delim, const F &f) {
			f(*first++);
			while (first != last) {
				ss.put(delim);
				f(*first++);
			}
		}

		template<typename Stream>
		void render_json_value(Stream& ss, nullptr_t) { ss.write("null"); }

		template<typename Stream>
		void render_json_value(Stream& ss, bool b) { ss.write(b ? "true" : "false"); };

		template<typename Stream, typename T>
		std::enable_if_t<!std::is_floating_point<T>::value&&(std::is_integral<T>::value || std::is_unsigned<T>::value || std::is_signed<T>::value)>
			render_json_value(Stream& ss, T value)
		{
			char temp[20];
			auto p = itoa_fwd(value, temp);
			ss.write(temp, p - temp);
		}

		template<typename Stream, typename T>
		std::enable_if_t<std::is_floating_point<T>::value> render_json_value(Stream& ss, T value)
		{
			char temp[20];
			sprintf(temp, "%f", value);
			ss.write(temp);
		}

		template<typename Stream>
		void render_json_value(Stream& ss, const std::string &s)
		{
			ss.put('"');
			ss.write(s.c_str(), s.size());
			ss.put('"');
		}

		template<typename Stream>
		void render_json_value(Stream& ss, const char* s, size_t size)
		{
			ss.put('"');
			ss.write(s, size);
			ss.put('"');
		}

		template<typename Stream, typename T>
		std::enable_if_t<std::is_arithmetic<T>::value> render_key(Stream& ss, T t) {
			ss.put('"');
			render_json_value(ss, t);
			ss.put('"');
		}

		template<typename Stream>
		void render_key(Stream& ss, const std::string &s) {
			render_json_value(ss, s);
		}

		template<typename Stream, typename T, typename = std::enable_if_t<is_reflection<T>::value>>
		void to_json(Stream& ss, T &&t);

		template<typename Stream, typename T, typename = std::enable_if_t<is_reflection<T>::value>>
		void render_json_value(Stream& ss, T &&t) {
			to_json(ss, std::forward<T>(t));
		}

		template<typename Stream, typename T>
		std::enable_if_t <std::is_enum<T>::value> render_json_value(Stream& ss, T val)
		{
			render_json_value(ss, (std::underlying_type_t<T>&)val);
		}

		template <typename Stream, typename T, size_t N>
		void render_json_value(Stream& ss, T(&v)[N])
		{
			ss.put('[');
			join(ss, std::begin(v), std::end(v), ',',
				[&ss](const auto &jsv) {
				render_json_value(ss, jsv);
			});
			ss.put(']');
		}

		template<typename Stream, typename T>
		std::enable_if_t <is_associat_container<T>::value>
			render_json_value(Stream& ss, const T&o) {
			ss.put('{');
			join(ss, o.cbegin(), o.cend(), ',',
				[&ss](const auto &jsv) {
				render_key(ss, jsv.first);
				ss.put(':');
				render_json_value(ss, jsv.second);
			});
			ss.put('}');
		}

		template<typename Stream, typename T>
		std::enable_if_t <is_sequence_container<T>::value> render_json_value(Stream& ss, const T &v) {
			ss.put('[');
			join(ss, v.cbegin(), v.cend(), ',',
				[&ss](const auto &jsv) {
				render_json_value(ss, jsv);
			});
			ss.put(']');
		}
		//template<typename Stream, typename T, typename = std::enable_if_t<is_reflection<T>::value>>
		//void write_json_key(Stream& s, size_t I)
		//{
		//	s.put('"');
		//	auto name = get_name<T>(I);
		//	s.write(name, strlen(name));
		//	s.put('"');
		//}

#define write_json_key(s, I)\
		s.put('"');\
		auto name = get_name<T>(I);\
		s.write(name, strlen(name));\
		s.put('"');

		template<typename Stream, typename T, typename = std::enable_if_t<is_reflection<T>::value>>
		void to_json(Stream& s, T &&t) {
			s.put('{');
			for_each(std::forward<T>(t),
				[&s](const auto &v, size_t I, bool is_last) { //magic for_each struct std::forward<T>(t)
				write_json_key(s, I);
				s.put(':');
				render_json_value(s, v);
				if (!is_last)
					s.put(',');
			}, [&s](const auto &o, size_t I, bool is_last) {
				write_json_key(s, I);
				s.put(':');
				to_json(s, o);
				if (!is_last)
					s.put(',');
			});
			s.put('}');

			//return s.str();
		}

		template<typename Stream, typename T>
		void to_json(Stream& s, const std::vector<T> &v) {
			render_json_value(v);
		};

		template<typename Stream, typename... Args>
		void to_json(Stream& s, std::tuple<Args...> tp) {
			s.put('[');
			apply_tuple([&s](const auto &v, size_t I, bool is_last) {
				render_json_value(s, v);
				if (!is_last)
					s.put(',');
			}, tp, std::make_index_sequence<sizeof...(Args)>{});
			s.put(']');
		}

		///***********************************  from json*********************************///
		//reader from ajson
		namespace detail {
			struct string_ref {
				char const *str;
				size_t len;

				int length() const {
					int size = len;
					for (size_t i = 0; i < len; ++i) {
						if (str[i] == 92 || str[i] == 116 || str[i] == 50)
							size -= 1;
					}

					return size;
				}

				bool operator!=(const char *data) const {
					const int str_length = strlen(data);
					if (len != str_length) {
						if (length() != str_length)
							return true;
					}

					for (int i = 0; i < str_length; ++i) {
						if (str[i] != data[i]) {
							return true;
						}
					}
					return false;
				}
			};
		}
		struct token {
			detail::string_ref str;
			enum {
				t_string,
				t_int,
				t_uint,
				t_number,
				t_ctrl,
				t_end,
			} type;
			union {
				int64_t i64;
				uint64_t u64;
				double d64;
			} value;
		};

		class reader_t {
		public:
			reader_t(const char *ptr = nullptr, size_t len = -1) : ptr_(ptr), len_(len) {
				if (ptr == nullptr) {
					end_mark_ = true;
				}
				else if (len == 0) {
					end_mark_ = true;
				}
				else if (ptr[0] == 0) {
					end_mark_ = true;
				}
				next();
			}

			static inline char *itoa_native(size_t val, char *buffer, size_t len) {
				buffer[len] = 0;
				size_t pos = len - 1;
				if (val == 0) {
					buffer[pos--] = '0';
				}
				while (val) {
					buffer[pos--] = (char)((val % 10) + '0');
					val = val / 10;
				}
				++pos;
				return &buffer[0] + pos;
			}

			inline void error(const char *message) const {
				char buffer[20];
				std::string msg = "error at line :";
				msg += itoa_native(cur_line_, buffer, 19);
				msg += " col :";
				msg += itoa_native(cur_col_, buffer, 19);
				msg += " msg:";
				msg += message;
				throw std::invalid_argument(msg);
			}

			inline token const &peek() const {
				return cur_tok_;
			}

			void next() {
				auto c = skip();
				bool do_next = false;
				switch (c) {
				case 0:
					cur_tok_.type = token::t_end;
					cur_tok_.str.str = ptr_ + cur_offset_;
					cur_tok_.str.len = 1;
					break;
				case '{':
				case '}':
				case '[':
				case ']':
				case ':':
				case ',': {
					cur_tok_.type = token::t_ctrl;
					cur_tok_.str.str = ptr_ + cur_offset_;
					cur_tok_.str.len = 1;
					take();
					break;
				}
				case '/': {
					take();
					c = read();
					if (c == '/') {
						take();
						c = read();
						while (c != '\n' && c != 0) {
							take();
							c = read();
						}
						do_next = true;
						break;
					}
					else if (c == '*') {
						take();
						c = read();
						do {
							while (c != '*') {
								if (c == 0) {
									return;
								}
								take();
								c = read();
							}
							take();
							c = read();
							if (c == '/') {
								take();
								do_next = true;
								break;
							}
						} while (true);
					}
					//error parser comment
					error("not a comment!");
				}
				case '"': {
					cur_tok_.type = token::t_string;
					parser_quote_string();
					break;
				}
				default: {
					if (c >= '0' && c <= '9') {
						cur_tok_.type = token::t_uint;
						cur_tok_.value.u64 = c - '0';
						parser_number();
					}
					else if (c == '-') {
						cur_tok_.type = token::t_int;
						cur_tok_.value.u64 = '0' - c;
						parser_number();
					}
					else {
						cur_tok_.type = token::t_string;
						parser_string();
					}
				}
				}
				if (do_next == false)
					return;
				next();
			}

			inline bool expect(char c) {
				return cur_tok_.str.str[0] == c;
			}

		private:
			inline void decimal_reset() { decimal = 0.1; }

			inline char read() const {
				if (end_mark_)
					return 0;
				return ptr_[cur_offset_];
			}

			inline void take() {
				if (end_mark_ == false) {
					++cur_offset_;
					char v = ptr_[cur_offset_];
					if (v != '\r')
						++cur_col_;
					if (len_ > 0 && cur_offset_ >= len_) {
						end_mark_ = true;
					}
					else if (v == 0) {
						end_mark_ = true;
					}
					if (v == '\n') {
						cur_col_ = 0;
						++cur_line_;
					}
				}
			}

			char skip() {
				auto c = read();
				while (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
					take();
					c = read();
				}
				return c;
			}

			inline void fill_escape_char(size_t count, char c) {
				//            if (count == 0)
				//                return;
				//
				//           ptr_[cur_offset_ - count] = c;
			}

			char table[103] = {
					16, 16, 16, 16, 16, 16, 16, 16, 16, 17, 17, 16, 17, 17, 16,
					16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
					16, 16, 17, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
					16, 16, 16, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 16, 16, 16, 16, 16,
					16, 16, 10, 11, 12, 13, 14, 15, 16, 16, 16, 16, 16, 16, 16, 16,
					16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
					16, 16, 10, 11, 12, 13, 14, 15 };

			inline char char_to_hex(char v) {
				if (v < 'f') {
					v = table[v];
				}
				else {
					v = 16;
				}
				if (v > 15)
					error("utf8 code error!");
				return v;
			}

			inline uint64_t read_utf() {
				char v = char_to_hex(read());
				uint64_t utf = v;
				utf <<= 4;
				take();
				v = char_to_hex(read());
				utf += v;
				utf <<= 4;
				take();
				v = char_to_hex(read());
				utf += v;
				utf <<= 4;
				take();
				v = char_to_hex(read());
				utf += v;
				take();
				return utf;
			}

			inline void esacpe_utf8(size_t &esc_count) {
				uint64_t utf1 = read_utf();
				esc_count += 6;

				if (utf1 < 0x80) {
					fill_escape_char(esc_count, (char)utf1);
				}
				else if (utf1 < 0x800) {
					fill_escape_char(esc_count, (char)(0xC0 | ((utf1 >> 6) & 0xFF)));
					fill_escape_char(esc_count - 1, (char)(0x80 | ((utf1 & 0x3F))));
				}
				else if (utf1 < 0x80000) {
					fill_escape_char(esc_count, (char)(0xE0 | ((utf1 >> 12) & 0xFF)));
					fill_escape_char(esc_count - 1, (char)(0x80 | ((utf1 >> 6) & 0x3F)));
					fill_escape_char(esc_count - 2, (char)(0x80 | ((utf1 & 0x3F))));
				}
				else {
					if (utf1 < 0x110000) {
						error("utf8 code error!");
					}
					fill_escape_char(esc_count, (char)(0xF0 | ((utf1 >> 18) & 0xFF)));
					fill_escape_char(esc_count - 1, (char)(0x80 | ((utf1 >> 12) & 0x3F)));
					fill_escape_char(esc_count - 2, (char)(0x80 | ((utf1 >> 6) & 0x3F)));
					fill_escape_char(esc_count - 3, (char)(0x80 | ((utf1 & 0x3F))));
				}
			}

			void parser_quote_string() {
				take();
				cur_tok_.str.str = ptr_ + cur_offset_;
				auto c = read();
				size_t esc_count = 0;
				do {
					switch (c) {
					case 0:
					case '\n': {
						error("not a valid quote string!");
						break;
					}
					case '\\': {
						take();
						c = read();
						switch (c) {
						case 'b': {
							c = '\b';
							break;
						}
						case 'f': {
							c = '\f';
							break;
						}
						case 'n': {
							c = '\n';
							break;
						}
						case 'r': {
							c = '\r';
							break;
						}
						case 't': {
							c = '\t';
							break;
						}
						case '"': {
							break;
						}
						case 'u': {
							take();
							esacpe_utf8(esc_count);
							continue;
						}
						default: {
							error("unknown escape char!");
						}
						}
						++esc_count;
						break;
					}
					case '"': {
						cur_tok_.str.len = ptr_ + cur_offset_ - esc_count - cur_tok_.str.str;
						take();
						return;
					}
					}
					fill_escape_char(esc_count, c);
					take();
					c = read();
				} while (true);
			}

			void parser_string() {
				cur_tok_.str.str = ptr_ + cur_offset_;
				take();
				auto c = read();
				size_t esc_count = 0;
				do {
					switch (c) {
					case 0:
					case '\n': {
						error("not a valid string!");
						break;
					}
					case '\\': {
						take();
						c = read();
						switch (c) {
						case 'b': {
							c = '\b';
							break;
						}
						case 'f': {
							c = '\f';
							break;
						}
						case 'n': {
							c = '\n';
							break;
						}
						case 'r': {
							c = '\r';
							break;
						}
						case 't': {
							c = '\t';
							break;
						}
						case 'u': {
							take();
							esacpe_utf8(esc_count);
							continue;
						}
						default: {
							error("unknown escape char!");
						}
						}
						++esc_count;
						break;
					}
					case ' ':
					case '\t':
					case '\r':
					case ',':
					case '[':
					case ']':
					case ':':
					case '{':
					case '}': {
						cur_tok_.str.len = ptr_ + cur_offset_ - esc_count - cur_tok_.str.str;
						return;
					}
					}
					fill_escape_char(esc_count, c);
					take();
					c = read();
				} while (true);
			}

			void parser_number() {
				cur_tok_.str.str = ptr_ + cur_offset_;
				take();
				auto c = read();
				do {
					switch (c) {
					case '0':
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7':
					case '8':
					case '9': {
						if (cur_tok_.type == token::t_int) {
							cur_tok_.value.i64 *= 10;
							cur_tok_.value.i64 += c - '0';
						}
						else if (cur_tok_.type == token::t_uint) {
							cur_tok_.value.u64 *= 10;
							cur_tok_.value.u64 += c - '0';
						}
						else if (cur_tok_.type == token::t_number) {
							cur_tok_.value.d64 += decimal * (c - '0');
							decimal *= 0.1;
						}
						break;
					}
					case '.': {
						if (cur_tok_.type == token::t_int) {
							cur_tok_.type = token::t_number;
							cur_tok_.value.d64 = (double)cur_tok_.value.i64;
							decimal_reset();
						}
						else if (cur_tok_.type == token::t_uint) {
							cur_tok_.type = token::t_number;
							cur_tok_.value.d64 = (double)cur_tok_.value.u64;
							decimal_reset();
						}
						else if (cur_tok_.type == token::t_number) {
							error("not a valid number!");
						}
						break;
					}
					default: {
						cur_tok_.str.len = ptr_ + cur_offset_ - cur_tok_.str.str;
						return;
					}
					}
					take();
					c = read();
				} while (1);
			}

			token cur_tok_;
			size_t cur_col_ = 0;
			size_t cur_line_ = 0;
			size_t len_ = 0;
			size_t cur_offset_ = 0;
			bool end_mark_ = false;
			const char *ptr_;
			double decimal = 0.1;
		};

		void skip(reader_t &rd);

		inline void skip_array(reader_t &rd) {
			auto tok = &rd.peek();
			while (tok->str.str[0] != ']') {
				skip(rd);
				tok = &rd.peek();
				if (tok->str.str[0] == ',') {
					rd.next();
					tok = &rd.peek();
					continue;
				}
			}
			rd.next();
		}

		inline void skip_key(reader_t &rd) {
			auto &tok = rd.peek();
			switch (tok.type) {
			case token::t_string:
			case token::t_int:
			case token::t_uint:
			case token::t_number: {
				rd.next();
				return;
			}
			default:
				break;
			}
			rd.error("invalid json document!");
		}

		inline void skip_object(reader_t &rd) {
			auto tok = &rd.peek();
			while (tok->str.str[0] != '}') {
				skip_key(rd);
				tok = &rd.peek();
				if (tok->str.str[0] == ':') {
					rd.next();
					skip(rd);
					tok = &rd.peek();
				}
				else {
					rd.error("invalid json document!");
				}
				if (tok->str.str[0] == ',') {
					rd.next();
					tok = &rd.peek();
					continue;
				}
			}
			rd.next();
		}

		inline void skip(reader_t &rd) {
			auto &tok = rd.peek();
			switch (tok.type) {
			case token::t_string:
			case token::t_int:
			case token::t_uint:
			case token::t_number: {
				rd.next();
				return;
			}
			case token::t_ctrl: {
				if (tok.str.str[0] == '[') {
					rd.next();
					skip_array(rd);
					return;
				}
				else if (tok.str.str[0] == '{') {
					rd.next();
					skip_object(rd);
					return;
				}
			}
			case token::t_end: {
				return;
			}
			}
			rd.error("invalid json document!");
		}

		//read json to value
		template<typename T>
		std::enable_if_t<is_signed_intergral_like<T>::value> read_json(reader_t &rd, T &val) {
			auto &tok = rd.peek();
			switch (tok.type) {
			case token::t_string: {
				int64_t temp = std::strtoll(tok.str.str, nullptr, 10);
				val = static_cast<int>(temp);
				break;
			}
			case token::t_int: {
				val = static_cast<int>(tok.value.i64);
				break;
			}
			case token::t_uint: {
				val = static_cast<int>(tok.value.u64);
				break;
			}
			case token::t_number: {
				val = static_cast<int>(tok.value.d64);
				break;
			}
			default: {
				rd.error("not a valid signed integral like number.");
			}
			}
			rd.next();
		}

		template<typename T>
		inline std::enable_if_t<is_unsigned_intergral_like<T>::value> read_json(reader_t &rd, T &val) {
			auto &tok = rd.peek();
			switch (tok.type) {
			case token::t_string: {
				uint64_t temp = std::strtoull(tok.str.str, nullptr, 10);
				val = static_cast<unsigned int>(temp);
				break;
			}
			case token::t_int: {
				if (tok.value.i64 < 0) {
					rd.error("assign a negative signed integral to unsigned integral number.");
				}
				val = static_cast<unsigned int>(tok.value.i64);
				break;
			}
			case token::t_uint: {
				val = static_cast<unsigned int>(tok.value.u64);
				break;
			}
			case token::t_number: {
				if (tok.value.d64 < 0) {
					rd.error("assign a negative float point to unsigned integral number.");
				}
				val = static_cast<unsigned int>(tok.value.d64);
				break;
			}
			default: {
				rd.error("not a valid unsigned integral like number.");
			}
			}
			rd.next();
		}

		template<typename T>
		inline std::enable_if_t<std::is_enum<T>::value> read_json(reader_t &rd, T &val) {
			typedef typename std::underlying_type<T>::type RAW_TYPE;
			read_json(rd, (RAW_TYPE&)val);
		}

		template<typename T>
		inline std::enable_if_t<std::is_floating_point<T>::value> read_json(reader_t &rd, T &val) {
			auto& tok = rd.peek();
			switch (tok.type)
			{
			case token::t_string:
			{
				double temp = std::strtold(tok.str.str, nullptr);
				val = static_cast<T>(temp);
				break;
			}
			case token::t_int:
			{
				val = static_cast<T>(tok.value.i64);
				break;
			}
			case token::t_uint:
			{
				val = static_cast<T>(tok.value.u64);
				break;
			}
			case token::t_number:
			{
				val = static_cast<T>(tok.value.d64);
				break;
			}
			default:
			{
				rd.error("not a valid float point number.");
			}
			}
			rd.next();
		}

		inline void read_json(reader_t &rd, bool &val) {
			auto& tok = rd.peek();
			switch (tok.type)
			{
			case token::t_string:
			{
				char const * ptr = tok.str.str;
				if (tok.str.len == 4)
				{
					val = (ptr[0] == 't' || ptr[0] == 'T') &&
						(ptr[1] == 'r' || ptr[1] == 'R') &&
						(ptr[2] == 'u' || ptr[2] == 'U') &&
						(ptr[3] == 'e' || ptr[3] == 'E');
				}
				else
				{
					val = false;
				}
				break;
			}
			case token::t_int:
			{
				val = (tok.value.i64 != 0);
				break;
			}
			case token::t_uint:
			{
				val = (tok.value.u64 != 0);
				break;
			}
			case token::t_number:
			{
				val = (tok.value.d64 != 0.0);
				break;
			}
			default:
			{
				rd.error("not a valid bool.");
			}
			}
			rd.next();
		}

		void read_json(reader_t &rd, std::string &val) {
			auto &tok = rd.peek();
			if (tok.type == token::t_string) {
				val.assign(tok.str.str, tok.str.len);
			}
			else {
				rd.error("not a valid string.");
			}
			rd.next();
		}

		template <typename T, size_t N>
		void read_json(reader_t &rd, T(&val)[N])
		{
			auto& tok = rd.peek();
			if (tok.type == token::t_string)
			{
				size_t len = tok.str.len;
				if (len > N)
					len = N;
				memcpy(val, tok.str.str, len);
				if (len < N)
					val[len] = 0;
			}
			else
			{
				rd.error("not a valid string.");
			}
			rd.next();
		}

		template<typename T>
		std::enable_if_t<is_emplace_back_able<T>::value> emplace_back(T &val) {
			val.emplace_back();
		}

		template<typename T>
		std::enable_if_t<is_template_instant_of<std::queue, T>::value> emplace_back(T &val) {
			val.emplace();
		}

		template<typename T>
		std::enable_if_t<is_sequence_container<T>::value> read_json(reader_t &rd, T &val) {
			if (rd.expect('[') == false) {
				rd.error("array must start with [.");
			}
			rd.next();
			auto tok = &rd.peek();
			while (tok->str.str[0] != ']') {
				emplace_back(val);
				read_json(rd, val.back());
				tok = &rd.peek();
				if (tok->str.str[0] == ',') {
					rd.next();
					tok = &rd.peek();
					continue;
				}
				else if (tok->str.str[0] == ']') {
					break;
				}
				else {
					rd.error("no valid array!");
				}
			}
			rd.next();
		}

		template<typename T>
		std::enable_if_t<is_associat_container<T>::value> read_json(reader_t &rd, T &val) {
			if (rd.expect('{') == false)
			{
				rd.error("object must start with {!");
			}
			rd.next();
			auto tok = &rd.peek();
			while (tok->str.str[0] != '}')
			{
				typename T::key_type key;
				read_json(rd, key);
				if (rd.expect(':') == false)
				{
					rd.error("invalid object!");
				}
				rd.next();
				typename T::mapped_type value;
				read_json(rd, value);
				val[key] = value;
				tok = &rd.peek();
				if (tok->str.str[0] == ',')
				{
					rd.next();
					tok = &rd.peek();
					continue;
				}
				else if (tok->str.str[0] == '}')
				{
					break;
				}
				else
				{
					rd.error("no valid object!");
				}
			}
			rd.next();
		}

		template<typename T, typename = std::enable_if_t<is_reflection<T>::value>>
		void read_json(reader_t &rd, T &val) {
			do_read(rd, val);
			rd.next();
		}

		template<typename T, typename = std::enable_if_t<is_reflection<T>::value>>
		void do_read(reader_t &rd, T &&t) {
			for_each(std::forward<T>(t), [&rd](auto &v, size_t I, bool is_last) {
				rd.next();
				if (rd.peek().str != get_name<T>(I))
					return;

				rd.next();
				rd.next();
				read_json(rd, v);
			}, [&rd](auto &o, size_t I, bool is_last) {
				rd.next();
				rd.next();
				rd.next();
				do_read(rd, o);
				rd.next();
			});
		}

		template<typename T, typename = std::enable_if_t<is_reflection<T>::value>>
		void from_json(T &&t, const char *buf, size_t len = -1) {
			reader_t rd(buf, len);
			do_read(rd, t);
		}
	}
}
#endif //SERIALIZE_JSON_HPP
