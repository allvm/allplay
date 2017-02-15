#ifndef BOOST_PROGRESS_H
#define BOOST_PROGRESS_H

// XXX:
// This code is from Boost 1.63
// http://www.boost.org/doc/libs/1_63_0/boost/progress.hpp
//
// Adapted for my use.

#include <string>
#include <llvm/Support/raw_ostream.h>

namespace boost {

struct noncopyable {};

class progress_display : private noncopyable
{
 public:
  explicit progress_display( unsigned long expected_count_,
                             llvm::raw_ostream & os = llvm::outs(),
                             const std::string & s1 = "\n", //leading strings
                             const std::string & s2 = "",
                             const std::string & s3 = "" )
   // os is hint; implementation may ignore, particularly in embedded systems
   : noncopyable(), m_os(os), m_s1(s1), m_s2(s2), m_s3(s3) { restart(expected_count_); }

  void           restart( unsigned long expected_count_ )
  //  Effects: display appropriate scale
  //  Postconditions: count()==0, expected_count()==expected_count_
  {
    _count = _next_tic_count = _tic = 0;
    _expected_count = expected_count_;

    m_os << m_s1 << "0%   10   20   30   40   50   60   70   80   90   100%\n"
         << m_s2 << "|----|----|----|----|----|----|----|----|----|----|"
         << "\n" // std::endl  // endl implies flush, which ensures display
         << m_s3;
    m_os.flush();
    if ( !_expected_count ) _expected_count = 1;  // prevent divide by zero
  } // restart

  unsigned long  operator+=( unsigned long increment )
  //  Effects: Display appropriate progress tic if needed.
  //  Postconditions: count()== original count() + increment
  //  Returns: count().
  {
    if ( (_count += increment) >= _next_tic_count ) { display_tic(); }
    return _count;
  }

  unsigned long  operator++()           { return operator+=( 1 ); }
  unsigned long  count() const          { return _count; }
  unsigned long  expected_count() const { return _expected_count; }

  private:
  llvm::raw_ostream &     m_os;  // may not be present in all imps
  const std::string  m_s1;  // string is more general, safer than 
  const std::string  m_s2;  //  const char *, and efficiency or size are
  const std::string  m_s3;  //  not issues

  unsigned long _count, _expected_count, _next_tic_count;
  unsigned int  _tic;
  void display_tic()
  {
    // use of floating point ensures that both large and small counts
    // work correctly.  static_cast<>() is also used several places
    // to suppress spurious compiler warnings. 
    unsigned int tics_needed = static_cast<unsigned int>((static_cast<double>(_count)
        / static_cast<double>(_expected_count)) * 50.0);
    do { m_os << '*'; m_os.flush(); } while ( ++_tic < tics_needed );
    _next_tic_count = 
      static_cast<unsigned long>((_tic/50.0) * static_cast<double>(_expected_count));
    if ( _count == _expected_count ) {
      if ( _tic < 51 ) m_os << '*';
      m_os << "\n"; m_os.flush();
      }
  } // display_tic
};

} // end namespace boost

#endif // BOOST_PROGRESS_H
