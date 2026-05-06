" NexusLang ftplugin for Neovim/Vim
" Loaded automatically when a .nlpl file is opened

if exists("b:did_ftplugin")
  finish
endif
let b:did_ftplugin = 1

" Indentation
setlocal expandtab
setlocal shiftwidth=4
setlocal tabstop=4
setlocal softtabstop=4

" Comment string
setlocal commentstring=#\ %s

" Fold method based on indentation (matches NLPL's indentation-based blocks)
setlocal foldmethod=indent
setlocal foldlevel=99

" Completion from current file + spell
setlocal complete+=k

" Smart indentation: indent after function, class, if, while, for, match, try
setlocal indentexpr=NLPLIndent()
setlocal indentkeys+=0=end,0=else,0=otherwise,0=case,0=catch,0=finally

function! NLPLIndent()
  let prev_lnum = prevnonblank(v:lnum - 1)
  if prev_lnum == 0
    return 0
  endif
  let prev_line = getline(prev_lnum)
  let indent = indent(prev_lnum)

  " Increase indent after block-opening keywords
  if prev_line =~# '^\s*\(function\|class\|struct\|union\|if\|while\|for\|match\|try\|do\|when\)\b'
    return indent + &shiftwidth
  endif
  if prev_line =~# '^\s*else\s*$\|^\s*otherwise\s*$\|^\s*case\b\|^\s*catch\b\|^\s*finally\b'
    return indent + &shiftwidth
  endif

  " Decrease indent at closing keywords
  let cur_line = getline(v:lnum)
  if cur_line =~# '^\s*\(end\|else\|otherwise\|case\|catch\|finally\)\b'
    return max([0, indent - &shiftwidth])
  endif

  return indent
endfunction

" Undo the ftplugin settings when the filetype changes
let b:undo_ftplugin = "setlocal et< sw< ts< sts< cms< fdm< fdl< cpt< inde< indk<"
