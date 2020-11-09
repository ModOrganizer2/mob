#pragma once

namespace mob
{

class console_color
{
public:
  enum colors
  {
	white,
	grey,
	yellow,
	red
  };

  console_color();
  console_color(colors c);
  ~console_color();

private:
  bool reset_;
  WORD old_atts_;
};


class u8stream
{
public:
  u8stream(bool err)
	: err_(err)
  {
  }

  template <class... Args>
  u8stream& operator<<(Args&&... args)
  {
	std::ostringstream oss;
	((oss << std::forward<Args>(args)), ...);

	do_output(oss.str());

	return *this;
  }

  void write_ln(std::string_view utf8);

private:
  bool err_;

  void do_output(const std::string& s);
};


extern u8stream u8cout;
extern u8stream u8cerr;

void set_std_streams();
std::mutex& global_output_mutex();


// see https://github.com/isanae/mob/issues/4
//
// this restores the original console font if it changed
//
class font_restorer
{
public:
  font_restorer()
	: restore_(false)
  {
	std::memset(&old_, 0, sizeof(old_));
	old_.cbSize = sizeof(old_);

	if (GetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &old_))
	  restore_ = true;
  }

  ~font_restorer()
  {
	if (!restore_)
	  return;

	CONSOLE_FONT_INFOEX now = {};
	now.cbSize = sizeof(now);

	if (!GetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &now))
	  return;

	if (std::wcsncmp(old_.FaceName, now.FaceName, LF_FACESIZE) != 0)
	  restore();
  }

  void restore()
  {
	::SetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &old_);
  }

private:
  CONSOLE_FONT_INFOEX old_;
  bool restore_;
};

} // namepsace
