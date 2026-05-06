package com.nlpl.intellij

import com.intellij.openapi.editor.colors.TextAttributesKey
import com.intellij.openapi.fileTypes.SyntaxHighlighter
import com.intellij.openapi.options.colors.AttributesDescriptor
import com.intellij.openapi.options.colors.ColorDescriptor
import com.intellij.openapi.options.colors.ColorSettingsPage
import javax.swing.Icon

/**
 * Provides the Colors & Fonts settings page for NexusLang under
 * Settings | Editor | Color Scheme | NexusLang.
 */
class NlplColorSettingsPage : ColorSettingsPage {

    private val DESCRIPTORS = arrayOf(
        AttributesDescriptor("Keyword",      NlplSyntaxHighlighter.KEYWORD),
        AttributesDescriptor("Built-in function", NlplSyntaxHighlighter.BUILTIN),
        AttributesDescriptor("Type",         NlplSyntaxHighlighter.TYPE),
        AttributesDescriptor("Constant",     NlplSyntaxHighlighter.CONSTANT),
        AttributesDescriptor("String",       NlplSyntaxHighlighter.STRING),
        AttributesDescriptor("Number",       NlplSyntaxHighlighter.NUMBER),
        AttributesDescriptor("Comment",      NlplSyntaxHighlighter.COMMENT),
        AttributesDescriptor("Doc comment",  NlplSyntaxHighlighter.DOC_COMMENT),
        AttributesDescriptor("Identifier",   NlplSyntaxHighlighter.IDENTIFIER),
        AttributesDescriptor("Function name", NlplSyntaxHighlighter.FUNCTION),
        AttributesDescriptor("Class name",   NlplSyntaxHighlighter.CLASS_NAME),
        AttributesDescriptor("Operator",     NlplSyntaxHighlighter.OPERATOR),
    )

    override fun getIcon(): Icon = NlplIcons.FILE

    override fun getHighlighter(): SyntaxHighlighter = NlplSyntaxHighlighter()

    override fun getDemoText(): String = """
## Calculate the area of shapes
# This is a regular comment

function calculate_circle_area with radius as Float returns Float
    set pi to 3.14159
    set area to pi times radius times radius
    return area
end

class Shape
    set name to "unnamed"

    function describe with self returns String
        set message to "I am a " + self name
        return message
    end
end

set my_radius to 5.0
set result to calculate_circle_area with radius: my_radius
print text "Area: " + result

if result is greater than 50.0
    print text "Large circle"
else
    print text "Small circle"
end

for each value in [1, 2, 3, true, false, null]
    print text value
end
""".trimIndent()

    override fun getAdditionalHighlightingTagToDescriptorMap(): Map<String, TextAttributesKey>? = null

    override fun getAttributeDescriptors(): Array<AttributesDescriptor> = DESCRIPTORS

    override fun getColorDescriptors(): Array<ColorDescriptor> = ColorDescriptor.EMPTY_ARRAY

    override fun getDisplayName(): String = "NexusLang"
}
