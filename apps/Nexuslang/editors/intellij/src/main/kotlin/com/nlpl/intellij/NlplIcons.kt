package com.nlpl.intellij

import com.intellij.openapi.util.IconLoader
import javax.swing.Icon

/**
 * Icon registry for the NexusLang plugin.
 * Place nlpl.svg (16x16) at resources/icons/nlpl.svg for the file icon.
 */
object NlplIcons {
    @JvmField
    val FILE: Icon = IconLoader.getIcon("/icons/nlpl.svg", NlplIcons::class.java)
}
