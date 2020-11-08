#include "FileIncluder.h"
#include "Common.h"
#include <iostream>
#include <fstream>

shaderc_include_result *MakeErrorIncludeResult( const char *message ) {
  return new shaderc_include_result{"", 0, message, strlen( message )};
}

//helper classes
class string_piece
{
public:
  typedef const char *iterator;
  static const size_t npos = -1;

  string_piece() {}

  string_piece( const char *begin, const char *end ) : begin_( begin ), end_( end )
  {
    assert( ( begin == nullptr ) == ( end == nullptr ) &&
	    "either both begin and end must be nullptr or neither must be" );
  }

  string_piece( const char *string ) : begin_( string ), end_( string )
  {
    if ( string )
      {
	end_ += strlen( string );
      }
  }

  string_piece( const std::string &str )
  {
    if ( !str.empty() )
      {
	begin_ = &( str.front() );
	end_ = &( str.back() ) + 1;
      }
  }

  string_piece( const string_piece &other )
  {
    begin_ = other.begin_;
    end_ = other.end_;
  }

  void clear()
  {
    begin_ = nullptr;
    end_ = nullptr;
  }

  const char *data() const { return begin_; }

  std::string str() const { return std::string( begin_, end_ ); }

  string_piece substr( size_t pos, size_t len = npos ) const
  {
    assert( len == npos || pos + len <= size() );
    return string_piece( begin_ + pos, len == npos ? end_ : begin_ + pos + len );
  }

  template <typename T>
  size_t find_first_not_matching( T callee )
  {
    for ( auto it = begin_; it != end_; ++it )
      {
	if ( !callee( *it ) )
	  {
	    return it - begin_;
	  }
      }
    return npos;
  }

  size_t find_first_not_of( const string_piece &to_search,
			    size_t pos = 0 ) const
  {
    if ( pos >= size() )
      {
	return npos;
      }
    for ( auto it = begin_ + pos; it != end_; ++it )
      {
	if ( to_search.find_first_of( *it ) == npos )
	  {
	    return it - begin_;
	  }
      }
    return npos;
  }

  size_t find_first_not_of( char to_search, size_t pos = 0 ) const
  {
    return find_first_not_of( string_piece( &to_search, &to_search + 1 ), pos );
  }

  size_t find_first_of( const string_piece &to_search, size_t pos = 0 ) const
  {
    if ( pos >= size() )
      {
	return npos;
      }
    for ( auto it = begin_ + pos; it != end_; ++it )
      {
	for ( char c : to_search )
	  {
	    if ( c == *it )
	      {
		return it - begin_;
	      }
	  }
      }
    return npos;
  }

  size_t find_first_of( char to_search, size_t pos = 0 ) const
  {
    return find_first_of( string_piece( &to_search, &to_search + 1 ), pos );
  }

  size_t find_last_of( const string_piece &to_search, size_t pos = npos ) const
  {
    if ( empty() ) return npos;
    if ( pos >= size() )
      {
	pos = size();
      }
    auto it = begin_ + pos + 1;
    do
      {
	--it;
	if ( to_search.find_first_of( *it ) != npos )
	  {
	    return it - begin_;
	  }
      } while ( it != begin_ );
    return npos;
  }

  size_t find_last_of( char to_search, size_t pos = npos ) const
  {
    return find_last_of( string_piece( &to_search, &to_search + 1 ), pos );
  }

  size_t find_last_not_of( const string_piece &to_search,
			   size_t pos = npos ) const
  {
    if ( empty() ) return npos;
    if ( pos >= size() )
      {
	pos = size();
      }
    auto it = begin_ + pos + 1;
    do
      {
	--it;
	if ( to_search.find_first_of( *it ) == npos )
	  {
	    return it - begin_;
	  }
      } while ( it != begin_ );
    return npos;
  }

  size_t find_last_not_of( char to_search, size_t pos = 0 ) const
  {
    return find_last_not_of( string_piece( &to_search, &to_search + 1 ), pos );
  }

  string_piece lstrip( const string_piece &chars_to_strip ) const
  {
    iterator begin = begin_;
    for ( ; begin < end_; ++begin )
      if ( chars_to_strip.find_first_of( *begin ) == npos ) break;
    if ( begin >= end_ ) return string_piece();
    return string_piece( begin, end_ );
  }

  string_piece rstrip( const string_piece &chars_to_strip ) const
  {
    iterator end = end_;
    for ( ; begin_ < end; --end )
      if ( chars_to_strip.find_first_of( *( end - 1 ) ) == npos ) break;
    if ( begin_ >= end ) return string_piece();
    return string_piece( begin_, end );
  }

  string_piece strip( const string_piece &chars_to_strip ) const { return lstrip( chars_to_strip ).rstrip( chars_to_strip ); }
  string_piece strip_whitespace() const { return strip( " \t\n\r\f\v" ); }
  const char &operator[]( size_t i ) const { return *( begin_ + i ); }
  bool operator==( const string_piece &other ) const
  {
    // Either end_ and _begin_ are nullptr or neither of them are.
    assert( ( ( end_ == nullptr ) == ( begin_ == nullptr ) ) );
    assert( ( ( other.end_ == nullptr ) == ( other.begin_ == nullptr ) ) );
    if ( size() != other.size() )
      {
	return false;
      }
    return ( memcmp( begin_, other.begin_, end_ - begin_ ) == 0 );
  }

  bool operator!=( const string_piece &other ) const
  {
    return !operator==( other );
  }

  iterator begin() const { return begin_; }
  iterator end() const { return end_; }

  const char &front() const
  {
    assert( !empty() );
    return *begin_;
  }

  const char &back() const
  {
    assert( !empty() );
    return *( end_ - 1 );
  }

  bool starts_with( const string_piece &other ) const
  {
    const char *iter = begin_;
    const char *other_iter = other.begin();
    while ( iter != end_ && other_iter != other.end() )
      {
	if ( *iter++ != *other_iter++ )
	  {
	    return false;
	  }
      }
    return other_iter == other.end();
  }

  size_t find( const string_piece &substr, size_t pos = 0 ) const
  {
    if ( empty() ) return npos;
    if ( pos >= size() ) return npos;
    if ( substr.empty() ) return 0;
    for ( auto it = begin_ + pos;
	  end() - it >= static_cast<decltype( end() - it )>( substr.size() ); ++it )
      {
	if ( string_piece( it, end() ).starts_with( substr ) ) return it - begin_;
      }
    return npos;
  }
  size_t find( char character, size_t pos = 0 ) const { return find_first_of( character, pos ); }
  bool empty() const { return begin_ == end_; }
  size_t size() const { return end_ - begin_; }
  std::vector<string_piece> get_fields( char delimiter,
					bool keep_delimiter = false ) const
  {
    std::vector<string_piece> fields;
    size_t first = 0;
    size_t field_break = find_first_of( delimiter );
    while ( field_break != npos )
      {
	fields.push_back( substr( first, field_break - first + keep_delimiter ) );
	first = field_break + 1;
	field_break = find_first_of( delimiter, first );
      }
    if ( size() - first > 0 )
      {
	fields.push_back( substr( first, size() - first ) );
      }
    return fields;
  }

  friend std::ostream &operator<<( std::ostream &os, const string_piece &piece );

private:
  // It is expected that begin_ and end_ will both be null or
  // they will both point to valid pieces of memory, but it is invalid
  // to have one of them being nullptr and the other not.
  string_piece::iterator begin_ = nullptr;
  string_piece::iterator end_ = nullptr;
};

inline std::ostream &operator<<( std::ostream &os, const string_piece &piece )
{
  // Either end_ and _begin_ are nullptr or neither of them are.
  assert( ( ( piece.end_ == nullptr ) == ( piece.begin_ == nullptr ) ) );
  if ( piece.end_ != piece.begin_ )
    {
      os.write( piece.begin_, piece.end_ - piece.begin_ );
    }
  return os;
}

inline bool operator==( const char *first, const string_piece second )
{
  return second == first;
}

inline bool operator!=( const char *first, const string_piece second )
{
  return !operator==( first, second );
}

std::string MaybeSlash( const string_piece &path ) {
  return ( path.empty() || path.back() == '/' ) ? "" : "/";
}

std::string FileFinder::FindReadableFilepath(
					     const std::string &filename ) const
{
  assert( !filename.empty() );
  static const auto for_reading = std::ios_base::in;
  std::filebuf opener;
  for ( const auto &prefix : search_path_ )
    {
      const std::string prefixed_filename =
	prefix + MaybeSlash( prefix ) + filename;
      if ( opener.open( prefixed_filename, for_reading ) ) return prefixed_filename;
    }
  return "";
}

std::string FileFinder::FindRelativeReadableFilepath(
						     const std::string &requesting_file, const std::string &filename ) const
{
  assert( !filename.empty() );

  string_piece dir_name( requesting_file );

  size_t last_slash = requesting_file.find_last_of( "/\\" );
  if ( last_slash != std::string::npos )
    {
      dir_name = string_piece( requesting_file.c_str(),
			       requesting_file.c_str() + last_slash );
    }

  if ( dir_name.size() == requesting_file.size() )
    {
      dir_name.clear();
    }

  static const auto for_reading = std::ios_base::in;
  std::filebuf opener;
  const std::string relative_filename =
    dir_name.str() + MaybeSlash( dir_name ) + filename;
  if ( opener.open( relative_filename, for_reading ) ) return relative_filename;

  return FindReadableFilepath( filename );
}

FileIncluder::~FileIncluder() = default;

shaderc_include_result *FileIncluder::GetInclude(
						 const char *requested_source, shaderc_include_type include_type,
						 const char *requesting_source, size_t )
{
  const std::string full_path =
    ( include_type == shaderc_include_type_relative )
    ? file_finder_.FindRelativeReadableFilepath( requesting_source,
						 requested_source )
    : file_finder_.FindReadableFilepath( requested_source );

  if ( full_path.empty() )
    return MakeErrorIncludeResult( "Cannot find or open include file." );

  // In principle, several threads could be resolving includes at the same
  // time.  Protect the included_files.

  const auto ReadFile = []( const std::string &input_file_name,
			    std::vector<char> *input_data ) -> bool {
    std::istream *stream = &std::cin;
    std::ifstream input_file;
    if ( input_file_name != "-" )
      {
	input_file.open( input_file_name, std::ios_base::binary );
	stream = &input_file;
	if ( input_file.fail() )
	  {
	    std::string errorMessage = std::string( "Cannot open input file: " ) + input_file_name;
	    err_log(errorMessage);
	    return false;
	  }
      }
    *input_data = std::vector<char>( ( std::istreambuf_iterator<char>( *stream ) ),
				     std::istreambuf_iterator<char>() );
    return true;
  };

  // Read the file and save its full path and contents into stable addresses.
  FileInfo *new_file_info = new FileInfo{full_path, {}};
  if ( !ReadFile( full_path, &( new_file_info->contents ) ) )
    {
      return MakeErrorIncludeResult( "Cannot read file" );
    }

  included_files_.insert( full_path );

  return new shaderc_include_result{
    new_file_info->full_path.data(), new_file_info->full_path.length(),
    new_file_info->contents.data(), new_file_info->contents.size(),
    new_file_info};
}

void FileIncluder::ReleaseInclude( shaderc_include_result *include_result ) {
  FileInfo *info = static_cast<FileInfo *>( include_result->user_data );
  delete info;
  delete include_result;
}

