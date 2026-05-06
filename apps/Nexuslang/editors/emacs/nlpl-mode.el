;;; nlpl-mode.el --- Major mode for the NexusLang programming language  -*- lexical-binding: t; -*-

;; Author: The No-Hands Company
;; Version: 0.1.0
;; Package-Requires: ((emacs "27.1") (lsp-mode "8.0"))
;; Keywords: languages nlpl
;; URL: https://github.com/Zajfan/NLPL

;; This file is NOT part of GNU Emacs.

;; Usage:
;;
;;   (add-to-list 'load-path "/path/to/NexusLang/editors/emacs")
;;   (require 'nlpl-mode)
;;
;; With use-package:
;;   (use-package nlpl-mode
;;     :load-path "/path/to/NexusLang/editors/emacs"
;;     :mode "\\.nxl\\'")
;;
;; With lsp-mode:
;;   (add-hook 'nlpl-mode-hook #'lsp-deferred)

;;; Code:

(require 'font-lock)
(require 'smie nil t)

;; ---------------------------------------------------------------------------
;; Syntax table
;; ---------------------------------------------------------------------------

(defvar nlpl-mode-syntax-table
  (let ((st (make-syntax-table)))
    ;; Comments: # starts a line comment
    (modify-syntax-entry ?# "<" st)
    (modify-syntax-entry ?\n ">" st)
    ;; String delimiters
    (modify-syntax-entry ?\" "\"" st)
    (modify-syntax-entry ?\' "\"" st)
    ;; Word constituents
    (modify-syntax-entry ?_ "w" st)
    ;; Punctuation
    (modify-syntax-entry ?. "." st)
    st)
  "Syntax table for `nlpl-mode'.")

;; ---------------------------------------------------------------------------
;; Font-lock keywords
;; ---------------------------------------------------------------------------

(defconst nlpl-keywords
  '("function" "class" "struct" "union" "end"
    "if" "else" "otherwise" "while" "for" "each" "in" "do"
    "return" "break" "continue"
    "import" "module" "from" "export"
    "new" "delete" "free" "allocate"
    "try" "catch" "raise" "throw" "finally"
    "match" "with" "case" "default"
    "and" "or" "not")
  "NLPL language keyword list.")

(defconst nlpl-natural-keywords
  '("set" "to" "is" "are" "has" "have" "let"
    "as" "returns" "that" "takes" "of" "called"
    "when" "target" "greater" "less" "than"
    "equal" "plus" "minus" "times" "divided" "by"
    "parallel" "async" "await" "spawn" "join"
    "address" "dereference" "sizeof")
  "NLPL natural-language keyword list.")

(defconst nlpl-types
  '("Integer" "Float" "String" "Boolean" "List" "Dict"
    "Void" "Optional" "Result" "Ok" "Err" "Null"
    "Rc" "Arc" "Box" "Weak" "RefCell" "Mutex" "RwLock"
    "Any")
  "NLPL built-in type names.")

(defconst nlpl-constants
  '("true" "false" "null" "none" "some")
  "NLPL constant literals.")

(defconst nlpl-font-lock-keywords
  (list
   ;; Doc comments (## prefix) - highest priority
   '("##.*$" . font-lock-doc-face)
   ;; Regular comments (# prefix)
   '("#[^#].*$\\|#$" . font-lock-comment-face)
   ;; String literals
   '("\"[^\"\\\\]*\\(?:\\\\.[^\"\\\\]*\\)*\"" . font-lock-string-face)
   '("'[^'\\\\]*\\(?:\\\\.[^'\\\\]*\\)*'" . font-lock-string-face)
   ;; Types (capitalised identifiers)
   (cons (regexp-opt nlpl-types 'words) 'font-lock-type-face)
   ;; Keywords
   (cons (regexp-opt nlpl-keywords 'words) 'font-lock-keyword-face)
   ;; Natural-language keywords
   (cons (regexp-opt nlpl-natural-keywords 'words) 'font-lock-builtin-face)
   ;; Constants
   (cons (regexp-opt nlpl-constants 'words) 'font-lock-constant-face)
   ;; Function definitions: "function NAME"
   '("\\bfunction\\s-+\\([a-zA-Z_][a-zA-Z0-9_]*\\)"
     (1 font-lock-function-name-face))
   ;; Class/struct/union definitions
   '("\\b\\(class\\|struct\\|union\\)\\s-+\\([a-zA-Z_][a-zA-Z0-9_]*\\)"
     (2 font-lock-type-face))
   ;; Variable declarations: "set NAME to"
   '("\\bset\\s-+\\([a-zA-Z_][a-zA-Z0-9_]*\\)\\s-+to\\b"
     (1 font-lock-variable-name-face))
   ;; Numbers
   '("\\b\\(0x[0-9A-Fa-f]+\\|0b[01]+\\|0o[0-7]+\\|[0-9]+\\(?:\\.[0-9]+\\)?\\)\\b"
     . font-lock-number-face))
  "Font-lock keywords for `nlpl-mode'.")

;; ---------------------------------------------------------------------------
;; Indentation
;; ---------------------------------------------------------------------------

(defcustom nlpl-indent-offset 4
  "Number of spaces for one indentation level in NexusLang."
  :type 'integer
  :group 'nlpl)

(defconst nlpl-indent-increase-re
  (regexp-opt '("function" "class" "struct" "union" "if" "else"
                "otherwise" "while" "for" "match" "try" "catch"
                "finally" "do" "when")
              'words)
  "Regex for lines that increase the indentation of the next line.")

(defconst nlpl-indent-decrease-re
  (regexp-opt '("end" "else" "otherwise" "case" "catch" "finally")
              'words)
  "Regex for lines that should be dedented relative to the previous block.")

(defun nlpl-indent-line ()
  "Indent current line as NexusLang code."
  (interactive)
  (let* ((pos (- (point-max) (point)))
         (indent (nlpl--calculate-indent)))
    (when (numberp indent)
      (beginning-of-line)
      (skip-chars-forward " \t")
      (let ((cur (current-column)))
        (unless (= cur indent)
          (delete-horizontal-space)
          (indent-to indent)))
      (when (> (- (point-max) pos) (point))
        (goto-char (- (point-max) pos))))))

(defun nlpl--calculate-indent ()
  "Calculate the expected indentation for the current line."
  (save-excursion
    (beginning-of-line)
    (let ((cur-line (buffer-substring-no-properties (point) (line-end-position))))
      (if (bobp)
          0
        (forward-line -1)
        (while (and (not (bobp))
                    (looking-at-p "\\s-*$"))
          (forward-line -1))
        (let* ((prev-line (buffer-substring-no-properties (point) (line-end-position)))
               (prev-indent (current-indentation))
               (increase (string-match nlpl-indent-increase-re (string-trim prev-line)))
               (decrease (string-match nlpl-indent-decrease-re (string-trim cur-line))))
          (cond
           ((and increase decrease) prev-indent)
           (increase (+ prev-indent nlpl-indent-offset))
           (decrease (max 0 (- prev-indent nlpl-indent-offset)))
           (t prev-indent)))))))

;; ---------------------------------------------------------------------------
;; LSP server registration
;; ---------------------------------------------------------------------------

(defun nlpl--find-lsp-server ()
  "Return the path to the NexusLang LSP server, or nil if not found."
  (let* ((python (or (executable-find "python3") (executable-find "python")))
         (cmd (when python
                (string-trim
                 (shell-command-to-string
                  (format "%s -c \"import nlpl, os; print(os.path.join(os.path.dirname(nlpl.__file__), 'lsp', '__main__.py'))\" 2>/dev/null"
                          python)))))
         (path (when (and cmd (file-readable-p cmd)) cmd)))
    path))

;;;###autoload
(defun nlpl-setup-lsp ()
  "Register the NexusLang LSP server with lsp-mode."
  (when (featurep 'lsp-mode)
    (let* ((python (or (executable-find "python3") (executable-find "python") "python3"))
           (server-path (nlpl--find-lsp-server))
           (cmd (if server-path
                    (list python server-path)
                  (list python "-m" "nexuslang.lsp"))))
      (lsp-register-client
       (make-lsp-client
        :new-connection (lsp-stdio-connection cmd)
        :activation-fn (lsp-activate-on "nlpl")
        :server-id 'nlpl-lsp
        :priority -1
        :language-id "nlpl")))))

;; ---------------------------------------------------------------------------
;; Eglot support (Emacs 29+)
;; ---------------------------------------------------------------------------

(defun nlpl-setup-eglot ()
  "Register NexusLang with eglot."
  (when (featurep 'eglot)
    (let* ((python (or (executable-find "python3") (executable-find "python") "python3")))
      (add-to-list 'eglot-server-programs
                   `(nlpl-mode . (,python "-m" "nexuslang.lsp"))))))

;; ---------------------------------------------------------------------------
;; Compilation support (M-x compile)
;; ---------------------------------------------------------------------------

(defcustom nlpl-build-command "nlpl build"
  "Command used by \\[nlpl-build]."
  :type 'string
  :group 'nlpl)

(defcustom nlpl-test-command "nlpl test"
  "Command used by \\[nlpl-test]."
  :type 'string
  :group 'nlpl)

(defun nlpl-build ()
  "Build the NexusLang project."
  (interactive)
  (compile nlpl-build-command))

(defun nlpl-test ()
  "Run NexusLang tests."
  (interactive)
  (compile nlpl-test-command))

(defun nlpl-coverage ()
  "Run coverage on the current NexusLang file."
  (interactive)
  (compile (format "nlpl coverage %s" (shell-quote-argument (buffer-file-name)))))

(defun nlpl-profile ()
  "Profile the current NexusLang file."
  (interactive)
  (compile (format "nlpl profile %s" (shell-quote-argument (buffer-file-name)))))

(defun nlpl-run ()
  "Build and run the NexusLang project."
  (interactive)
  (compile "nlpl run"))

;; ---------------------------------------------------------------------------
;; Mode keymap
;; ---------------------------------------------------------------------------

(defvar nlpl-mode-map
  (let ((map (make-sparse-keymap)))
    (define-key map (kbd "C-c C-b") #'nlpl-build)
    (define-key map (kbd "C-c C-t") #'nlpl-test)
    (define-key map (kbd "C-c C-r") #'nlpl-run)
    (define-key map (kbd "C-c C-c") #'nlpl-coverage)
    (define-key map (kbd "C-c C-p") #'nlpl-profile)
    map)
  "Keymap for `nlpl-mode'.")

;; ---------------------------------------------------------------------------
;; Major mode definition
;; ---------------------------------------------------------------------------

;;;###autoload
(define-derived-mode nlpl-mode prog-mode "NexusLang"
  "Major mode for editing NexusLang source code.

Key bindings:
\\{nlpl-mode-map}"
  :syntax-table nlpl-mode-syntax-table
  (setq-local font-lock-defaults '(nlpl-font-lock-keywords nil nil nil nil))
  (setq-local comment-start "# ")
  (setq-local comment-end "")
  (setq-local comment-start-skip "#+\\s-*")
  (setq-local indent-line-function #'nlpl-indent-line)
  (setq-local tab-width nlpl-indent-offset)
  (setq-local indent-tabs-mode nil)
  ;; Multi-line string support via syntax highlighting
  (setq-local font-lock-multiline t)
  ;; Electric indent
  (setq-local electric-indent-inhibit t)
  ;; Setup LSP clients if available
  (nlpl-setup-lsp)
  (nlpl-setup-eglot))

;;;###autoload
(add-to-list 'auto-mode-alist '("\\.nxl\\'" . nlpl-mode))

(provide 'nlpl-mode)

;;; nlpl-mode.el ends here
