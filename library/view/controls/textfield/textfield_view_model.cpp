
#include "textfield_view_model.h"

#include "base/logging.h"

#include "gfx/font.h"

#include "../../base/range.h"
#include "../../clipboard/scoped_clipboard_writer.h"
#include "../../view/view_delegate.h"

namespace view
{

    TextfieldViewModel::Delegate::~Delegate() {}

    TextfieldViewModel::TextfieldViewModel(Delegate* delegate)
        : delegate_(delegate),
        cursor_pos_(0),
        selection_start_(0),
        composition_start_(0),
        composition_end_(0),
        is_password_(false) {}

    TextfieldViewModel::~TextfieldViewModel() {}

    void TextfieldViewModel::GetFragments(TextFragments* fragments) const
    {
        DCHECK(fragments);
        fragments->clear();
        if(HasCompositionText())
        {
            if(composition_start_)
            {
                fragments->push_back(TextFragment(0, composition_start_,
                    false, false));
            }

            size_t selection_start = std::min(selection_start_, cursor_pos_);
            size_t selection_end = std::max(selection_start_, cursor_pos_);
            size_t last_end = composition_start_;
            for(CompositionUnderlines::const_iterator i=
                composition_underlines_.begin();
                i!=composition_underlines_.end(); ++i)
            {
                size_t fragment_start = std::min(i->start_offset, i->end_offset) +
                    composition_start_;
                size_t fragment_end = std::max(i->start_offset, i->end_offset) +
                    composition_start_;

                fragment_start = std::max(last_end, fragment_start);
                fragment_end = std::min(fragment_end, composition_end_);

                if(fragment_start >= fragment_end)
                {
                    break;
                }

                // If there is no selection, then just add a text fragment with underline.
                if(selection_start == selection_end)
                {
                    if(last_end < fragment_start)
                    {
                        fragments->push_back(TextFragment(last_end, fragment_start,
                            false, false));
                    }
                    fragments->push_back(TextFragment(fragment_start, fragment_end,
                        false, true));
                    last_end = fragment_end;
                    continue;
                }

                size_t end = std::min(fragment_start, selection_start);
                if(last_end < end)
                {
                    fragments->push_back(TextFragment(last_end, end, false,
                        false));
                }

                last_end = fragment_end;

                if(selection_start < fragment_start)
                {
                    end = std::min(selection_end, fragment_start);
                    fragments->push_back(TextFragment(selection_start, end,
                        true, false));
                    selection_start = end;
                }
                else if(selection_start > fragment_start)
                {
                    end = std::min(selection_start, fragment_end);
                    fragments->push_back(TextFragment(fragment_start, end,
                        false, true));
                    fragment_start = end;
                    if(fragment_start == fragment_end)
                    {
                        continue;
                    }
                }

                if(fragment_start < selection_end)
                {
                    DCHECK_EQ(selection_start, fragment_start);
                    end = std::min(fragment_end, selection_end);
                    fragments->push_back(TextFragment(fragment_start, end,
                        true, true));
                    fragment_start = end;
                    selection_start = end;
                    if(fragment_start == fragment_end)
                    {
                        continue;
                    }
                }

                DCHECK_LT(fragment_start, fragment_end);
                fragments->push_back(TextFragment(fragment_start, fragment_end,
                    false, true));
            }

            if(last_end < composition_end_)
            {
                if (selection_start < selection_end)
                {
                    DCHECK_LE(last_end, selection_start);
                    if(last_end < selection_start)
                    {
                        fragments->push_back(TextFragment(last_end, selection_start,
                            false, false));
                    }
                    fragments->push_back(TextFragment(selection_start, selection_end,
                        true, false));
                    if(selection_end < composition_end_)
                    {
                        fragments->push_back(TextFragment(selection_end,
                            composition_end_, false, false));
                    }
                }
                else
                {
                    fragments->push_back(TextFragment(last_end, composition_end_,
                        false, false));
                }
            }

            size_t len = text_.length();
            if(composition_end_ < len)
            {
                fragments->push_back(TextFragment(composition_end_, len,
                    false, false));
            }
        }
        else if(HasSelection())
        {
            size_t start = std::min(selection_start_, cursor_pos_);
            size_t end = std::max(selection_start_, cursor_pos_);
            if(start)
            {
                fragments->push_back(TextFragment(0, start, false, false));
            }
            fragments->push_back(TextFragment(start, end, true, false));
            size_t len = text_.length();
            if(end != len)
            {
                fragments->push_back(TextFragment(end, len, false, false));
            }
        }
        else
        {
            fragments->push_back(TextFragment(0, text_.length(), false, false));
        }
    }

    bool TextfieldViewModel::SetText(const string16& text)
    {
        bool changed = false;
        if(HasCompositionText())
        {
            ConfirmCompositionText();
            changed = true;
        }
        if(text_ != text)
        {
            text_ = text;
            if(cursor_pos_ > text.length())
            {
                cursor_pos_ = text.length();
            }
            changed = true;
        }
        ClearSelection();
        return changed;
    }

    void TextfieldViewModel::InsertText(const string16& text)
    {
        if(HasCompositionText())
        {
            ClearCompositionText();
        }
        else if(HasSelection())
        {
            DeleteSelection();
        }
        text_.insert(cursor_pos_, text);
        cursor_pos_ += text.size();
        ClearSelection();
    }

    void TextfieldViewModel::ReplaceText(const string16& text)
    {
        if(HasCompositionText())
        {
            ClearCompositionText();
        }
        else if(!HasSelection())
        {
            SelectRange(Range(cursor_pos_, cursor_pos_+text.length()));
        }
        InsertText(text);
    }

    void TextfieldViewModel::Append(const string16& text)
    {
        if(HasCompositionText())
        {
            ConfirmCompositionText();
        }
        text_ += text;
    }

    bool TextfieldViewModel::Delete()
    {
        if(HasCompositionText())
        {
            ClearCompositionText();
            return true;
        }
        if(HasSelection())
        {
            DeleteSelection();
            return true;
        }
        if(text_.length() > cursor_pos_)
        {
            text_.erase(cursor_pos_, 1);
            return true;
        }
        return false;
    }

    bool TextfieldViewModel::Backspace()
    {
        if(HasCompositionText())
        {
            ClearCompositionText();
            return true;
        }
        if(HasSelection())
        {
            DeleteSelection();
            return true;
        }
        if(cursor_pos_ > 0)
        {
            cursor_pos_--;
            text_.erase(cursor_pos_, 1);
            ClearSelection();
            return true;
        }
        return false;
    }

    void TextfieldViewModel::MoveCursorLeft(bool select)
    {
        if(HasCompositionText())
        {
            ConfirmCompositionText();
        }
        // TODO(oshima): support BIDI
        if(select)
        {
            if(cursor_pos_ > 0)
            {
                cursor_pos_--;
            }
        }
        else
        {
            if(HasSelection())
            {
                cursor_pos_ = std::min(cursor_pos_, selection_start_);
            }
            else if(cursor_pos_ > 0)
            {
                cursor_pos_--;
            }
            ClearSelection();
        }
    }

    void TextfieldViewModel::MoveCursorRight(bool select)
    {
        if(HasCompositionText())
        {
            ConfirmCompositionText();
        }
        // TODO(oshima): support BIDI
        if(select)
        {
            cursor_pos_ = std::min(text_.length(),  cursor_pos_+1);
        }
        else
        {
            if(HasSelection())
            {
                cursor_pos_ = std::max(cursor_pos_, selection_start_);
            }
            else
            {
                cursor_pos_ = std::min(text_.length(),  cursor_pos_+1);
            }
            ClearSelection();
        }
    }

    void TextfieldViewModel::MoveCursorToPreviousWord(bool select)
    {
        if(HasCompositionText())
        {
            ConfirmCompositionText();
        }
        // WLW TODO: fix it.
        //// Notes: We always iterate words from the begining.
        //// This is probably fast enough for our usage, but we may
        //// want to modify WordIterator so that it can start from the
        //// middle of string and advance backwards.
        //base::BreakIterator iter(&text_, base::BreakIterator::BREAK_WORD);
        //bool success = iter.Init();
        //DCHECK(success);
        //if(!success)
        //{
        //    return;
        //}
        //int last = 0;
        //while(iter.Advance())
        //{
        //    if(iter.IsWord())
        //    {
        //        size_t begin = iter.pos() - iter.GetString().length();
        //        if(begin == cursor_pos_)
        //        {
        //            // The cursor is at the beginning of a word.
        //            // Move to previous word.
        //            break;
        //        }
        //        else if(iter.pos() >= cursor_pos_)
        //        {
        //            // The cursor is in the middle or at the end of a word.
        //            // Move to the top of current word.
        //            last = begin;
        //            break;
        //        }
        //        else
        //        {
        //            last = iter.pos() - iter.GetString().length();
        //        }
        //    }
        //}

        //cursor_pos_ = last;
        //if(!select)
        //{
        //    ClearSelection();
        //}
    }

    void TextfieldViewModel::MoveCursorToNextWord(bool select)
    {
        if(HasCompositionText())
        {
            ConfirmCompositionText();
        }
        // WLW TODO: fix it.
        //base::BreakIterator iter(&text_, base::BreakIterator::BREAK_WORD);
        //bool success = iter.Init();
        //DCHECK(success);
        //if(!success)
        //{
        //    return;
        //}
        //size_t pos = 0;
        //while(iter.Advance())
        //{
        //    pos = iter.pos();
        //    if(iter.IsWord() && pos > cursor_pos_)
        //    {
        //        break;
        //    }
        //}
        //cursor_pos_ = pos;
        //if(!select)
        //{
        //    ClearSelection();
        //}
    }

    void TextfieldViewModel::MoveCursorToHome(bool select)
    {
        if(HasCompositionText())
        {
            ConfirmCompositionText();
        }
        cursor_pos_ = 0;
        if(!select)
        {
            ClearSelection();
        }
    }

    void TextfieldViewModel::MoveCursorToEnd(bool select)
    {
        if(HasCompositionText())
        {
            ConfirmCompositionText();
        }
        cursor_pos_ = text_.length();
        if(!select)
        {
            ClearSelection();
        }
    }

    bool TextfieldViewModel::MoveCursorTo(size_t pos, bool select)
    {
        if(HasCompositionText())
        {
            ConfirmCompositionText();
        }
        bool cursor_changed = false;
        if(cursor_pos_ != pos)
        {
            cursor_pos_ = pos;
            cursor_changed = true;
        }
        if(!select)
        {
            ClearSelection();
        }
        return cursor_changed;
    }

    gfx::Rect TextfieldViewModel::GetCursorBounds(const gfx::Font& font) const
    {
        string16 text = GetVisibleText();
        int x = font.GetStringWidth(text.substr(0U, cursor_pos_));
        int h = font.GetHeight();
        DCHECK(x >= 0);
        if(text.length() == cursor_pos_)
        {
            return gfx::Rect(x, 0, 0, h);
        }
        else
        {
            int x_end = font.GetStringWidth(text.substr(0U, cursor_pos_+1U));
            return gfx::Rect(x, 0, x_end-x, h);
        }
    }

    string16 TextfieldViewModel::GetSelectedText() const
    {
        return text_.substr(std::min(cursor_pos_, selection_start_),
            std::abs(static_cast<long>(cursor_pos_-selection_start_)));
    }

    void TextfieldViewModel::GetSelectedRange(Range* range) const
    {
        *range = Range(selection_start_, cursor_pos_);
    }

    void TextfieldViewModel::SelectRange(const Range& range)
    {
        if(HasCompositionText())
        {
            ConfirmCompositionText();
        }
        selection_start_ = GetSafePosition(range.start());
        cursor_pos_ = GetSafePosition(range.end());
    }

    void TextfieldViewModel::SelectAll()
    {
        if(HasCompositionText())
        {
            ConfirmCompositionText();
        }
        // SelectAll selects towards the end.
        cursor_pos_ = text_.length();
        selection_start_ = 0;
    }

    void TextfieldViewModel::SelectWord()
    {
        if(HasCompositionText())
        {
            ConfirmCompositionText();
        }
        // First we setup selection_begin_ and cursor_pos_. There are so many cases
        // because we try to emulate what select-word looks like in a gtk textfield.
        // See associated testcase for different cases.
        if(cursor_pos_>0 && cursor_pos_<text_.length())
        {
            if(isalnum(text_[cursor_pos_]))
            {
                selection_start_ = cursor_pos_;
                cursor_pos_++;
            }
            else
            {
                selection_start_ = cursor_pos_ - 1;
            }
        }
        else if(cursor_pos_ == 0)
        {
            selection_start_ = cursor_pos_;
            if(text_.length() > 0)
            {
                cursor_pos_++;
            }
        }
        else
        {
            selection_start_ = cursor_pos_ - 1;
        }

        // Now we move selection_begin_ to beginning of selection. Selection boundary
        // is defined as the position where we have alpha-num character on one side
        // and non-alpha-num char on the other side.
        for(; selection_start_>0; --selection_start_)
        {
            if(IsPositionAtWordSelectionBoundary(selection_start_))
            {
                break;
            }
        }

        // Now we move cursor_pos_ to end of selection. Selection boundary
        // is defined as the position where we have alpha-num character on one side
        // and non-alpha-num char on the other side.
        for(; cursor_pos_<text_.length(); ++cursor_pos_)
        {
            if(IsPositionAtWordSelectionBoundary(cursor_pos_))
            {
                break;
            }
        }
    }

    void TextfieldViewModel::ClearSelection()
    {
        if(HasCompositionText())
        {
            ConfirmCompositionText();
        }
        selection_start_ = cursor_pos_;
    }

    bool TextfieldViewModel::Cut()
    {
        if(!HasCompositionText() && HasSelection())
        {
            ScopedClipboardWriter(
                ViewDelegate::view_delegate->GetClipboard()).WriteText(
                GetSelectedText());
            DeleteSelection();
            return true;
        }
        return false;
    }

    void TextfieldViewModel::Copy()
    {
        if(!HasCompositionText() && HasSelection())
        {
            ScopedClipboardWriter(
                ViewDelegate::view_delegate->GetClipboard()).WriteText(
                GetSelectedText());
        }
    }

    bool TextfieldViewModel::Paste()
    {
        string16 result;
        ViewDelegate::view_delegate->GetClipboard()->ReadText(
            Clipboard::BUFFER_STANDARD, &result);
        if(!result.empty())
        {
            if(HasCompositionText())
            {
                ConfirmCompositionText();
            }
            else if(HasSelection())
            {
                DeleteSelection();
            }
            text_.insert(cursor_pos_, result);
            cursor_pos_ += result.length();
            ClearSelection();
            return true;
        }
        return false;
    }

    bool TextfieldViewModel::HasSelection() const
    {
        return selection_start_ != cursor_pos_;
    }

    void TextfieldViewModel::DeleteSelection()
    {
        DCHECK(!HasCompositionText());
        DCHECK(HasSelection());
        size_t n = std::abs(static_cast<long>(cursor_pos_ - selection_start_));
        size_t begin = std::min(cursor_pos_, selection_start_);
        text_.erase(begin, n);
        cursor_pos_ = begin;
        ClearSelection();
    }

    string16 TextfieldViewModel::GetTextFromRange(const Range& range) const
    {
        if(range.IsValid() && range.GetMin()<text_.length())
        {
            return text_.substr(range.GetMin(), range.length());
        }
        return string16();
    }

    void TextfieldViewModel::GetTextRange(Range* range) const
    {
        *range = Range(0, text_.length());
    }

    void TextfieldViewModel::SetCompositionText(
        const CompositionText& composition)
    {
        if(HasCompositionText())
        {
            ClearCompositionText();
        }
        else if(HasSelection())
        {
            DeleteSelection();
        }

        if(composition.text.empty())
        {
            return;
        }

        size_t length = composition.text.length();
        text_.insert(cursor_pos_, composition.text);
        composition_start_ = cursor_pos_;
        composition_end_ = composition_start_ + length;
        composition_underlines_ = composition.underlines;

        if(composition.selection.IsValid())
        {
            selection_start_ =
                std::min(composition_start_ + composition.selection.start(),
                composition_end_);
            cursor_pos_ =
                std::min(composition_start_ + composition.selection.end(),
                composition_end_);
        }
        else
        {
            cursor_pos_ = composition_end_;
            ClearSelection();
        }
    }

    void TextfieldViewModel::ConfirmCompositionText()
    {
        DCHECK(HasCompositionText());
        cursor_pos_ = composition_end_;
        composition_start_ = composition_end_ = string16::npos;
        composition_underlines_.clear();
        ClearSelection();
        if(delegate_)
        {
            delegate_->OnCompositionTextConfirmedOrCleared();
        }
    }

    void TextfieldViewModel::ClearCompositionText()
    {
        DCHECK(HasCompositionText());
        text_.erase(composition_start_, composition_end_-composition_start_);
        cursor_pos_ = composition_start_;
        composition_start_ = composition_end_ = string16::npos;
        composition_underlines_.clear();
        ClearSelection();
        if(delegate_)
        {
            delegate_->OnCompositionTextConfirmedOrCleared();
        }
    }

    void TextfieldViewModel::GetCompositionTextRange(Range* range) const
    {
        if(HasCompositionText())
        {
            *range = Range(composition_start_, composition_end_);
        }
        else
        {
            *range = Range::InvalidRange();
        }
    }

    bool TextfieldViewModel::HasCompositionText() const
    {
        return composition_start_ != composition_end_;
    }

    string16 TextfieldViewModel::GetVisibleText(size_t begin, size_t end) const
    {
        DCHECK(end >= begin);
        if(is_password_)
        {
            return string16(end-begin, '*');
        }
        return text_.substr(begin, end-begin);
    }

    bool TextfieldViewModel::IsPositionAtWordSelectionBoundary(size_t pos)
    {
        return (isalnum(text_[pos-1]) && !isalnum(text_[pos])) ||
            (!isalnum(text_[pos-1]) && isalnum(text_[pos]));
    }

    size_t TextfieldViewModel::GetSafePosition(size_t position) const
    {
        if(position > text_.length())
        {
            return text_.length();
        }
        return position;
    }

} //namespace view