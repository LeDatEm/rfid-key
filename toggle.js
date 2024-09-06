document.addEventListener("DOMContentLoaded", function () {
  const mode = localStorage.getItem("mode");
  if (mode === "dark") {
    document.body.classList.add("dark");
  }

  const sidebarStatus = localStorage.getItem("sidebar");
  if (sidebarStatus === "closed") {
    const sidebar = document.querySelector("#sidebar");
    sidebar.classList.add("hide");
  }

  const switchMode = document.getElementById("switch-mode");
  const switchModeStatus = localStorage.getItem("switchMode");
  if (switchModeStatus === "dark") {
    switchMode.checked = true;
  }

  switchMode.addEventListener("click", function () {
    if (document.body.classList.contains("dark")) {
      document.body.classList.remove("dark");
      localStorage.setItem("mode", "light");
      localStorage.setItem("switchMode", "light");
    } else {
      document.body.classList.add("dark");
      localStorage.setItem("mode", "dark");
      localStorage.setItem("switchMode", "dark");
    }
  });

  const menuIcon = document.querySelector(".bx-menu");
  const sidebar = document.querySelector("#sidebar");

  menuIcon.addEventListener("click", function () {
    const sidebarStatus = localStorage.getItem("sidebar");
    if (sidebarStatus === "closed" || !sidebarStatus) {
      sidebar.classList.remove("hide");
      localStorage.setItem("sidebar", "opened");
      console.log("mở");
    } else {
      sidebar.classList.add("hide");
      localStorage.setItem("sidebar", "closed");
      console.log("đóng");
    }
  });
});
