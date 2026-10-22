# KDESK interface refresh

Improve the native PC-98 desktop's visual hierarchy and window navigation while
preserving normal KDESK, K386 guest execution and the existing 12 KiB stack guard.

Planned scope:

- Refined palette, window frames, title controls, selection and desktop artwork.
- One taskbar for all open windows, including minimized windows and DOS sessions.
- Maximize/restore through a title control, title double-click and keyboard.
- Mouse-accessible DOS session launch and clearer interaction hints.
- Preserve partial presentation, pixel dragging, scrollbars and all existing apps.

Validate native builds and behavior tests, then both ordinary and K386 images in
WebNP2/Playwright. Record screenshots, limitations, source pins and CI results in
the paired PRs. Do not treat this UI work as completion of general DOS virtualization.
