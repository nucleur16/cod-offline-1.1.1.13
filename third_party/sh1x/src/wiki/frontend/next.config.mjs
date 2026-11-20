/** @type {import('next').NextConfig} */
const nextConfig = {
  reactStrictMode: true,
  // The wiki reads the server source tree; nothing it renders is user-uploaded,
  // so the default image pipeline is unused and left off.
  images: { unoptimized: true },
  output: 'standalone',
  eslint: { ignoreDuringBuilds: true },
};

export default nextConfig;
