(function () {
  const installCode = document.getElementById("install-code");
  const installTabs = Array.from(document.querySelectorAll("#install-tabs .i-tab"));
  const copyButton = document.getElementById("copy-install-btn");
  const emailInput = document.querySelector('.email-form input[type="email"]');
  const emailButton = document.querySelector(".email-form button");
  const nav = document.querySelector("nav");
  const navToggle = document.getElementById("nav-toggle");

  const commandSets = {
    unix: [
      { prompt: "#", cmd: "install once globally", muted: true },
      { prompt: "$", cmd: "npm install -g lovelang-runtime" },
      { prompt: "#", cmd: "run your first program", muted: true, gap: true },
      { prompt: "$", cmd: "lovelang /full/path/to/lovelang/examples/01-romantic-hello.love" },
      { prompt: "#", cmd: "with a mode", muted: true, gap: true },
      { prompt: "$", cmd: "lovelang /full/path/to/lovelang/examples/01-romantic-hello.love --mode shayari" }
    ],
    windows: [
      { prompt: "#", cmd: "install once globally", muted: true },
      { prompt: "PS>", cmd: "npm install -g lovelang-runtime" },
      { prompt: "#", cmd: "run your first program", muted: true, gap: true },
      { prompt: "PS>", cmd: "lovelang C:\\path\\to\\lovelang\\examples\\01-romantic-hello.love" },
      { prompt: "#", cmd: "with a mode", muted: true, gap: true },
      { prompt: "PS>", cmd: "lovelang C:\\path\\to\\lovelang\\examples\\01-romantic-hello.love --mode toxic" }
    ]
  };

  let activeOS = "unix";

  function escapeHtml(value) {
    return String(value)
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/\"/g, "&quot;")
      .replace(/'/g, "&#39;");
  }

  function linesToText(lines) {
    return lines
      .filter(function (line) { return !line.muted; })
      .map(function (line) { return line.cmd; })
      .join("\n");
  }

  function renderInstallCode(os) {
    if (!installCode || !commandSets[os]) {
      return;
    }

    installCode.innerHTML = commandSets[os]
      .map(function (line) {
        const cmdStyle = line.muted ? ' style="color:var(--muted)"' : "";
        const gapStyle = line.gap ? ' style="margin-top:8px"' : "";
        return (
          '<div class="install-line"' +
          gapStyle +
          '><span class="prompt">' +
          escapeHtml(line.prompt) +
          '</span><span class="cmd"' +
          cmdStyle +
          ">" +
          escapeHtml(line.cmd) +
          "</span></div>"
        );
      })
      .join("");
  }

  function setActiveTab(os) {
    activeOS = os;
    installTabs.forEach(function (tab) {
      tab.classList.toggle("active", tab.dataset.os === os);
    });
    renderInstallCode(os);
    if (copyButton) {
      copyButton.innerHTML = '<i class="ri-file-copy-line"></i> Copy commands';
    }
  }

  async function copyCommands() {
    if (!copyButton) {
      return;
    }

    const lines = commandSets[activeOS] || [];
    const text = linesToText(lines);

    try {
      if (!navigator.clipboard || !navigator.clipboard.writeText) {
        throw new Error("Clipboard API unavailable");
      }
      await navigator.clipboard.writeText(text);
      copyButton.innerHTML = '<i class="ri-check-line"></i> Copied!';
    } catch (err) {
      copyButton.innerHTML = '<i class="ri-close-line"></i> Copy failed';
      window.console.warn("[lovelang] copy failed", err);
    }
  }

  function subscribeNewsletter() {
    if (!emailInput || !emailButton) {
      return;
    }

    const email = emailInput.value.trim();
    if (!email || !email.includes("@")) {
      emailButton.innerHTML = '<i class="ri-error-warning-line"></i> Enter valid email';
      return;
    }

    emailButton.innerHTML = '<i class="ri-heart-fill"></i> Subscribed';
    emailInput.value = "";
  }

  function wireMobileNav() {
    if (!nav || !navToggle) {
      return;
    }

    function setExpanded(expanded) {
      const icon = navToggle.querySelector("i");
      navToggle.setAttribute("aria-expanded", expanded ? "true" : "false");
      if (icon) {
        icon.className = expanded ? "ri-close-line" : "ri-menu-line";
      }
    }

    function closeMenu() {
      nav.classList.remove("mobile-open");
      setExpanded(false);
    }

    navToggle.addEventListener("click", function () {
      const expanded = nav.classList.toggle("mobile-open");
      setExpanded(expanded);
    });

    nav.querySelectorAll("a").forEach(function (link) {
      link.addEventListener("click", function () {
        if (window.innerWidth <= 900) {
          closeMenu();
        }
      });
    });

    window.addEventListener("resize", function () {
      if (window.innerWidth > 900) {
        closeMenu();
      }
    });

    setExpanded(false);
  }

  installTabs.forEach(function (tab) {
    tab.addEventListener("click", function () {
      setActiveTab(tab.dataset.os || "unix");
    });
  });

  if (copyButton) {
    copyButton.addEventListener("click", copyCommands);
  }

  if (emailButton) {
    emailButton.addEventListener("click", subscribeNewsletter);
  }

  if (emailInput) {
    emailInput.addEventListener("keydown", function (event) {
      if (event.key === "Enter") {
        event.preventDefault();
        subscribeNewsletter();
      }
    });
  }

  wireMobileNav();
  setActiveTab(activeOS);
})();
