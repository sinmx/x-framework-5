
#ifndef __view_text_input_client_h__
#define __view_text_input_client_h__

#pragma once

#include "base/callback.h"
#include "base/rtl.h"

#include "../base/range.h"
#include "composition_text.h"
#include "text_input_type.h"

namespace gfx
{
    class Rect;
}

namespace view
{

    class View;

    // An interface implemented by a View that needs text input support.
    class TextInputClient
    {
    public:
        virtual ~TextInputClient() {}

        // Input method result -------------------------------------------------------

        // Sets composition text and attributes. If there is composition text already,
        // it��ll be replaced by the new one. Otherwise, current selection will be
        // replaced. If there is no selection, the composition text will be inserted
        // at the insertion point.
        virtual void SetCompositionText(const CompositionText& composition) = 0;

        // Converts current composition text into final content.
        virtual void ConfirmCompositionText() = 0;

        // Removes current composition text.
        virtual void ClearCompositionText() = 0;

        // Inserts a given text at the insertion point. Current composition text or
        // selection will be removed. This method should never be called when the
        // current text input type is TEXT_INPUT_TYPE_NONE.
        virtual void InsertText(const string16& text) = 0;

        // Inserts a single char at the insertion point. Unlike above InsertText()
        // method, this method has an extra |flags| parameter indicating the modifier
        // key states when the character is generated. This method should only be
        // called when a key press is not handled by the input method but still
        // generates a character (eg. by the keyboard driver). In another word, the
        // preceding key press event should not be a VKEY_PROCESSKEY.
        // This method will be called whenever a char is generated by the keyboard,
        // even if the current text input type is TEXT_INPUT_TYPE_NONE.
        virtual void InsertChar(char16 ch, int flags) = 0;

        // Input context information -------------------------------------------------

        // Returns current text input type. It could be changed and even becomes
        // TEXT_INPUT_TYPE_NONE at runtime.
        virtual TextInputType GetTextInputType() = 0;

        // Returns current caret (insertion point) bounds relative to the View��s
        // coordinates. If there is selection, then the selection bounds will be
        // returned.
        virtual gfx::Rect GetCaretBounds() = 0;

        // Returns true if there is composition text.
        virtual bool HasCompositionText() = 0;

        // Document content operations ----------------------------------------------

        // Retrieves the UTF-16 based character range containing all text in the View.
        // Returns false if the information cannot be retrieved right now.
        virtual bool GetTextRange(Range* range) = 0;

        // Retrieves the UTF-16 based character range of current composition text.
        // Returns false if the information cannot be retrieved right now.
        virtual bool GetCompositionTextRange(Range* range) = 0;

        // Retrieves the UTF-16 based character range of current selection.
        // Returns false if the information cannot be retrieved right now.
        virtual bool GetSelectionRange(Range* range) = 0;

        // Selects the given UTF-16 based character range. Current composition text
        // will be confirmed before selecting the range.
        // Returns false if the operation is not supported.
        virtual bool SetSelectionRange(const Range& range) = 0;

        // Deletes contents in the given UTF-16 based character range. Current
        // composition text will be confirmed before deleting the range.
        // The input caret will be moved to the place where the range gets deleted.
        // Returns false if the oepration is not supported.
        virtual bool DeleteRange(const Range& range) = 0;

        // Retrieves the text content in a given UTF-16 based character range.
        // The result will be send back to the input method by calling the given
        // callback, which may happen asynchronously.
        // Returns false if the operation is not supported.
        virtual bool GetTextFromRange(const Range& range,
            const base::Callback<void(const string16&)>& callback) = 0;

        // Miscellaneous ------------------------------------------------------------

        // Called whenever current keyboard layout or input method is changed,
        // especially the change of input locale and text direction.
        virtual void OnInputMethodChanged() = 0;

        // Called whenever the user requests to change the text direction and layout
        // alignment of the current text box. It��s for supporting ctrl-shift on
        // Windows.
        // Returns false if the operation is not supported.
        virtual bool ChangeTextDirectionAndLayoutAlignment(
            base::TextDirection direction) = 0;

        // Gets the View object who owns this TextInputClient instance.
        virtual View* GetOwnerViewOfTextInputClient() = 0;
    };

} //namespace view

#endif //__view_text_input_client_h__