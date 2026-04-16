// hyprmark.js — browser-side glue.
//
// Responsibilities:
//   - decorate code blocks with a copy button, run highlight.js on them
//   - convert ```mermaid fences (emitted by md4c as <pre><code class="language-mermaid">)
//     into <div class="mermaid"> and run mermaid on them
//   - run KaTeX auto-render for $ ... $ / $$ ... $$ math
//   - connect to the C++ host over qwebchannel and expose swap entry points
//
// Wire-up is idempotent: swapContent() re-runs the whole decoration pass.

(function () {
    'use strict';

    const HM = window.hyprmark = window.hyprmark || {};

    // Apply all decorations within `root` (default: document).
    function decorate(root) {
        root = root || document;

        // Convert ```mermaid fences → <div class="mermaid">
        root.querySelectorAll('pre > code.language-mermaid').forEach((code) => {
            const pre = code.parentElement;
            const div = document.createElement('div');
            div.className = 'mermaid';
            div.textContent = code.textContent;
            pre.replaceWith(div);
        });

        // Wrap every <pre><code> in a .code-block div with a copy button
        root.querySelectorAll('pre > code').forEach((code) => {
            const pre = code.parentElement;
            if (pre.parentElement && pre.parentElement.classList.contains('code-block'))
                return; // already decorated
            const wrap = document.createElement('div');
            wrap.className = 'code-block';
            pre.replaceWith(wrap);
            wrap.appendChild(pre);

            const btn = document.createElement('button');
            btn.className = 'copy-btn';
            btn.type = 'button';
            btn.textContent = 'Copy';
            btn.addEventListener('click', () => {
                const text = code.innerText;
                if (HM.host && typeof HM.host.copyToClipboard === 'function') {
                    HM.host.copyToClipboard(text);
                } else if (navigator.clipboard) {
                    navigator.clipboard.writeText(text).catch(() => {});
                }
                btn.classList.add('copied');
                btn.textContent = 'Copied';
                setTimeout(() => { btn.classList.remove('copied'); btn.textContent = 'Copy'; }, 1500);
            });
            wrap.appendChild(btn);
        });

        // highlight.js
        if (window.hljs) {
            root.querySelectorAll('pre code').forEach((el) => {
                try { window.hljs.highlightElement(el); } catch (e) { /* no-op */ }
            });
        }

        // mermaid — run on any fresh .mermaid divs we produced above
        if (window.mermaid) {
            try {
                window.mermaid.initialize({
                    startOnLoad: false,
                    theme: (getComputedStyle(document.documentElement).getPropertyValue('--hm-mermaid-theme') || 'default').trim() || 'default',
                });
                window.mermaid.run({ nodes: root.querySelectorAll('.mermaid:not([data-processed])') });
            } catch (e) {
                console.error('[hyprmark] mermaid failed:', e);
            }
        }

        // KaTeX auto-render
        if (window.renderMathInElement) {
            try {
                window.renderMathInElement(root, {
                    delimiters: [
                        { left: '$$', right: '$$', display: true },
                        { left: '$',  right: '$',  display: false },
                    ],
                    throwOnError: false,
                });
            } catch (e) {
                console.error('[hyprmark] katex auto-render failed:', e);
            }
        }

        // Report headings to the host for the TOC.
        if (HM.host && typeof HM.host.reportHeadings === 'function') {
            const headings = [];
            root.querySelectorAll('h1,h2,h3,h4,h5,h6').forEach((h) => {
                headings.push({ id: h.id || '', text: h.innerText, level: parseInt(h.tagName.substring(1), 10) });
            });
            try { HM.host.reportHeadings(JSON.stringify(headings)); } catch (e) { /* no-op */ }
        }
    }

    // In-place content swap invoked from C++ via QWebChannel.
    HM.swapContent = function (newHtml, sourcePath) {
        console.log('[hyprmark.js] swapContent called, len=' + (newHtml || '').length);
        const main = document.getElementById('hyprmark-content');
        if (!main) { console.error('[hyprmark.js] swapContent: #hyprmark-content not found'); return; }
        if (sourcePath) main.setAttribute('data-source', sourcePath);
        main.innerHTML = newHtml;
        decorate(main);
    };

    HM.swapTheme = function (url) {
        console.log('[hyprmark.js] swapTheme called with url=' + url);
        const link = document.getElementById('theme-stylesheet');
        console.log('[hyprmark.js] theme link found? ' + !!link + ', current href=' + (link && link.href));
        if (link) link.setAttribute('href', url);
        console.log('[hyprmark.js] theme link new href=' + (link && link.href));
        // Rerun mermaid so diagram palettes update to match the new theme.
        if (window.mermaid) {
            try {
                // Recompute each diagram from its source textContent cache.
                document.querySelectorAll('.mermaid').forEach((d) => {
                    if (d.dataset.src) {
                        d.removeAttribute('data-processed');
                        d.innerHTML = d.dataset.src;
                    } else if (!d.dataset.processed) {
                        d.dataset.src = d.textContent;
                    }
                });
                window.mermaid.initialize({
                    startOnLoad: false,
                    theme: (getComputedStyle(document.documentElement).getPropertyValue('--hm-mermaid-theme') || 'default').trim() || 'default',
                });
                window.mermaid.run({ nodes: document.querySelectorAll('.mermaid:not([data-processed])') });
            } catch (e) { /* no-op */ }
        }
    };

    HM.setScale = function (factor) {
        document.body.style.zoom = String(factor);
    };

    // Hook up QWebChannel → set HM.host = the C++ bridge.
    function attachChannel() {
        if (typeof QWebChannel === 'undefined' || !window.qt || !window.qt.webChannelTransport)
            return;
        new QWebChannel(window.qt.webChannelTransport, (channel) => {
            HM.host = channel.objects.hyprmarkHost || null;
        });
    }

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', () => { attachChannel(); decorate(document); console.log('[hyprmark.js] DOMContentLoaded: HM installed'); });
    } else {
        attachChannel();
        decorate(document);
        console.log('[hyprmark.js] immediate install: HM defined');
    }
})();
