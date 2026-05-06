-- NexusLang Language Support for Neovim
-- =================================
--
-- Installation (lazy.nvim):
--   {
--     dir = "/path/to/NexusLang/editors/neovim",
--     name = "nexuslang.nvim",
--     config = function() require("nlpl").setup() end,
--   }
--
-- Installation (packer.nvim):
--   use { "/path/to/NexusLang/editors/neovim", config = function()
--     require("nlpl").setup()
--   end }
--
-- Manual installation:
--   Copy this directory to ~/.config/nvim/pack/nlpl/start/nlpl.nvim/
--
-- Features:
--   - Syntax highlighting
--   - LSP integration (uses the NexusLang language server)
--   - File type detection
--   - Indentation rules
--   - Comment string configuration
--   - Key mappings (optional)

local M = {}

-- Default configuration
M.defaults = {
  -- Path to the NexusLang LSP server script (auto-detected if nil)
  lsp_server_path = nil,

  -- Python executable to use for running the LSP server
  python_cmd = "python3",

  -- Enable LSP integration (requires nvim-lspconfig)
  lsp_enable = true,

  -- Enable tree-sitter syntax (falls back to regex highlighting)
  treesitter = false,

  -- Indentation width
  indent_width = 4,

  -- Enable key mappings
  keymaps = true,

  -- Extra LSP settings
  lsp_settings = {},

  -- Callback invoked after LSP attaches
  on_attach = nil,
}

M.config = {}

-- ---------------------------------------------------------------------------
-- File type detection
-- ---------------------------------------------------------------------------
local function register_filetype()
  vim.filetype.add({
    extension = {
      nlpl = "nlpl",
    },
    filename = {
      ["build.nxl"] = "nlpl",
    },
    pattern = {
      [".*%.nxl"] = "nlpl",
    },
  })
end

-- ---------------------------------------------------------------------------
-- Syntax highlighting (Vim regex, no tree-sitter required)
-- ---------------------------------------------------------------------------
local syntax_script = [[
if exists("b:current_syntax") | finish | endif

syn case match

" Keywords
syn keyword nlplKeyword function class struct union end
syn keyword nlplKeyword if else otherwise while for each in do
syn keyword nlplKeyword return break continue
syn keyword nlplKeyword import module from export
syn keyword nlplKeyword new delete free allocate
syn keyword nlplKeyword try catch raise throw finally
syn keyword nlplKeyword match with case default
syn keyword nlplKeyword and or not

" Natural-language keywords
syn keyword nlplNatural set to is are has have let
syn keyword nlplNatural as returns that takes of called
syn keyword nlplNatural when target otherwise greater less than
syn keyword nlplNatural equal plus minus times divided by
syn keyword nlplNatural parallel async await spawn join

" Types
syn keyword nlplType Integer Float String Boolean List Dict
syn keyword nlplType Void Optional Result Ok Err Null
syn keyword nlplType Rc Arc Box Weak RefCell Mutex RwLock

" Constants
syn keyword nlplConstant true false null none some

" String literals
syn region nlplString start=/"/ skip=/\\"/ end=/"/ contains=nlplStringInterp
syn region nlplString start=/'/ skip=/\\'/ end=/'/
syn region nlplMultilineString start=/"""/ end=/"""/
syn match  nlplStringInterp /\${[^}]*}/ contained

" Numbers
syn match nlplNumber /\<[0-9]\+\(\.[0-9]\+\)\?\>/
syn match nlplNumber /\<0x[0-9A-Fa-f]\+\>/
syn match nlplNumber /\<0b[01]\+\>/
syn match nlplNumber /\<0o[0-7]\+\>/

" Comments
syn match  nlplComment /#[^#].*/ contains=nlplTodo
syn match  nlplDocComment /##.*/
syn keyword nlplTodo TODO FIXME HACK NOTE XXX WARN contained

" Identifiers and function calls
syn match nlplFunction /\<[a-z_][a-zA-Z0-9_]*\ze\s\+with\>/
syn match nlplIdentifier /\<[a-zA-Z_][a-zA-Z0-9_]*\>/

" Operators
syn match nlplOperator /[+\-*/<>=!&|^~%]/
syn match nlplOperator /\.\./
syn match nlplDelimiter /[{}()\[\],;]/

" Default highlight links
hi def link nlplKeyword     Keyword
hi def link nlplNatural     Statement
hi def link nlplType        Type
hi def link nlplConstant    Constant
hi def link nlplString      String
hi def link nlplMultilineString String
hi def link nlplStringInterp Special
hi def link nlplNumber      Number
hi def link nlplComment     Comment
hi def link nlplDocComment  SpecialComment
hi def link nlplTodo        Todo
hi def link nlplFunction    Function
hi def link nlplIdentifier  Identifier
hi def link nlplOperator    Operator
hi def link nlplDelimiter   Delimiter

let b:current_syntax = "nlpl"
]]

local function install_syntax()
  -- Write the syntax file to the Neovim runtime path
  local syntax_dir = vim.fn.stdpath("data") .. "/site/syntax"
  vim.fn.mkdir(syntax_dir, "p")
  local syntax_path = syntax_dir .. "/nlpl.vim"
  local f = io.open(syntax_path, "w")
  if f then
    f:write(syntax_script)
    f:close()
  end

  -- Also set up indentation and comment settings via autocmd
  vim.api.nvim_create_autocmd("FileType", {
    pattern = "nlpl",
    callback = function(ev)
      local opts = { buffer = ev.buf }
      vim.bo[ev.buf].commentstring = "# %s"
      vim.bo[ev.buf].expandtab = true
      vim.bo[ev.buf].shiftwidth = M.config.indent_width or 4
      vim.bo[ev.buf].tabstop = M.config.indent_width or 4
      vim.bo[ev.buf].softtabstop = M.config.indent_width or 4
    end,
  })
end

-- ---------------------------------------------------------------------------
-- LSP integration
-- ---------------------------------------------------------------------------
local function find_lsp_server()
  if M.config.lsp_server_path then
    return M.config.lsp_server_path
  end

  -- Look for the NexusLang package installed via pip
  local handle = io.popen("python3 -c \"import nlpl; import os; print(os.path.dirname(nlpl.__file__))\" 2>/dev/null")
  if handle then
    local nxl_dir = handle:read("*l")
    handle:close()
    if nxl_dir and nxl_dir ~= "" then
      local main_path = nxl_dir .. "/lsp/__main__.py"
      if vim.fn.filereadable(main_path) == 1 then
        return main_path
      end
    end
  end

  -- Fallback: look in common dev locations
  local candidates = {
    vim.fn.expand("~/NexusLang/src/nlpl/lsp/__main__.py"),
    vim.fn.expand("~/.local/lib/nlpl/lsp/__main__.py"),
  }
  for _, path in ipairs(candidates) do
    if vim.fn.filereadable(path) == 1 then
      return path
    end
  end

  vim.notify(
    "[nlpl.nvim] Could not find NexusLang LSP server. Set lsp_server_path in setup().",
    vim.log.levels.WARN
  )
  return nil
end

local function setup_lsp()
  local ok_lspconfig, lspconfig = pcall(require, "lspconfig")
  if not ok_lspconfig then
    vim.notify("[nlpl.nvim] nvim-lspconfig not found. LSP features disabled.", vim.log.levels.WARN)
    return
  end

  local server_path = find_lsp_server()
  if not server_path then
    return
  end

  local python = M.config.python_cmd or "python3"
  local cmd = { python, "-m", "nexuslang.lsp" }
  -- If server_path points to __main__.py directly, use it
  if server_path:match("%.py$") then
    cmd = { python, server_path }
  end

  -- Register nlpl in lspconfig's server_configurations
  local configs = require("lspconfig.configs")
  if not configs.nlpl then
    configs.nlpl = {
      default_config = {
        cmd = cmd,
        filetypes = { "nlpl" },
        root_dir = lspconfig.util.root_pattern("nexuslang.toml", ".git"),
        single_file_support = true,
        settings = M.config.lsp_settings or {},
      },
    }
  end

  lspconfig.nlpl.setup({
    on_attach = function(client, bufnr)
      -- Default key mappings
      if M.config.keymaps then
        local km = vim.keymap.set
        local bo = { buffer = bufnr, silent = true }
        km("n", "gd",         vim.lsp.buf.definition,     vim.tbl_extend("force", bo, { desc = "NLPL: Go to definition" }))
        km("n", "K",          vim.lsp.buf.hover,          vim.tbl_extend("force", bo, { desc = "NLPL: Hover documentation" }))
        km("n", "gi",         vim.lsp.buf.implementation, vim.tbl_extend("force", bo, { desc = "NLPL: Go to implementation" }))
        km("n", "<C-k>",      vim.lsp.buf.signature_help, vim.tbl_extend("force", bo, { desc = "NLPL: Signature help" }))
        km("n", "<leader>rn", vim.lsp.buf.rename,         vim.tbl_extend("force", bo, { desc = "NLPL: Rename symbol" }))
        km("n", "<leader>ca", vim.lsp.buf.code_action,    vim.tbl_extend("force", bo, { desc = "NLPL: Code action" }))
        km("n", "gr",         vim.lsp.buf.references,     vim.tbl_extend("force", bo, { desc = "NLPL: Find references" }))
        km("n", "<leader>f",  function() vim.lsp.buf.format({ async = true }) end, vim.tbl_extend("force", bo, { desc = "NLPL: Format" }))
      end

      -- User callback
      if M.config.on_attach then
        M.config.on_attach(client, bufnr)
      end
    end,
    capabilities = (function()
      local ok_cmp, cmp_lsp = pcall(require, "cmp_nvim_lsp")
      if ok_cmp then
        return cmp_lsp.default_capabilities()
      end
      return vim.lsp.protocol.make_client_capabilities()
    end)(),
    settings = M.config.lsp_settings or {},
  })
end

-- ---------------------------------------------------------------------------
-- User commands
-- ---------------------------------------------------------------------------
local function register_commands()
  vim.api.nvim_create_user_command("NLPLBuild", function(opts)
    local release = opts.args:find("--release") ~= nil
    local cmd = "nlpl build" .. (release and " --release" or "")
    vim.cmd("terminal " .. cmd)
  end, { nargs = "?", desc = "Build the NexusLang project" })

  vim.api.nvim_create_user_command("NLPLTest", function(opts)
    local coverage = opts.args:find("--coverage") ~= nil
    local cmd = "nlpl test" .. (coverage and " --coverage" or "")
    vim.cmd("terminal " .. cmd)
  end, { nargs = "?", desc = "Run NexusLang tests" })

  vim.api.nvim_create_user_command("NLPLRun", function()
    vim.cmd("terminal nlpl run")
  end, { desc = "Build and run the NexusLang project" })

  vim.api.nvim_create_user_command("NLPLCoverage", function(opts)
    local file = opts.args ~= "" and opts.args or vim.fn.expand("%")
    vim.cmd("terminal nlpl coverage " .. vim.fn.shellescape(file))
  end, { nargs = "?", desc = "Run NexusLang coverage on a file" })

  vim.api.nvim_create_user_command("NLPLProfile", function(opts)
    local file = opts.args ~= "" and opts.args or vim.fn.expand("%")
    vim.cmd("terminal nlpl profile " .. vim.fn.shellescape(file))
  end, { nargs = "?", desc = "Profile an NexusLang file" })

  vim.api.nvim_create_user_command("NLPLDoc", function(opts)
    local dir = opts.args ~= "" and opts.args or "docs/api"
    vim.cmd("terminal nlpl doc --output " .. vim.fn.shellescape(dir))
  end, { nargs = "?", desc = "Generate NexusLang API docs" })
end

-- ---------------------------------------------------------------------------
-- Main setup
-- ---------------------------------------------------------------------------
function M.setup(opts)
  M.config = vim.tbl_deep_extend("force", M.defaults, opts or {})

  -- File type detection
  register_filetype()

  -- Syntax highlighting
  install_syntax()

  -- LSP
  if M.config.lsp_enable then
    setup_lsp()
  end

  -- User commands
  register_commands()
end

return M
