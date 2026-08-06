# OptiNotch website

Static branding site for the OptiNotch Google OAuth consent screen, hosted on
GitHub Pages from the `docs/` folder on `main`.

| Page | URL (after enabling Pages) |
|------|----------------------------|
| Homepage | `https://aymanbdo2023-cloud.github.io/OptiNotch/` |
| Privacy policy | `https://aymanbdo2023-cloud.github.io/OptiNotch/privacy-policy.html` |

## Deploy

1. Commit and push the `docs/` folder to `main`.
2. In the GitHub repo go to **Settings → Pages**.
3. Under **Build and deployment → Source**, choose **Deploy from a branch**.
4. Set **Branch** to `main` and **folder** to `/docs`, then **Save**.
5. Wait a minute or two — Pages prints the site URL at the top of the settings page.

## Register with Google (OAuth consent screen)

1. Go to **Google Cloud Console → APIs & Services → OAuth consent screen**.
2. Under *Branding*, paste the homepage URL above and the privacy policy URL above,
   and upload an app logo (the notch mockup exported as PNG works).
3. Under *Audience*, set the app to **External** (public).
4. Because the app uses the sensitive scope `calendar.readonly`, publishing for
   public users requires **Google app verification** — the site URLs must be live
   and reachable before you submit. You can keep using the app in "Testing" mode
   (up to 100 test users) while verification is in progress.

## Edit

- `index.html` — homepage (hero, features, download button → GitHub Releases).
- `privacy-policy.html` — privacy policy (contact: `aycas2004@gmail.com`).
- `css/style.css` — shared styles; accent color is `#5C93FF` (matches the app default).
- `img/notch.svg` — hand-drawn mockup of the expanded overlay, used as hero image and favicon.

Pure static HTML/CSS — no build step, no dependencies.
