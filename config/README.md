# config — nginx Configuration

nginx reverse proxy configuration for PiFridge.


## Overview

This folder contains the nginx server block configuration that ties the PiFridge web layer together. nginx serves the static frontend (`index.html`) and routes API requests to the two FastCGI processes over Unix sockets.

The configuration file is automatically copied to the correct location by `run.sh` — manual setup is only needed if you are configuring the system by hand.



## Files

| File | Purpose |
|---|---|
| `pifridge.conf` | nginx server block — static file serving and FastCGI routing |



## Routing

| URL | Routed to |
|---|---|
| `/` | Serves `index.html` from `/var/www/pifridge` |
| `/api/fridge` | `pifridge_api` via `/var/run/pifridge/pifridge.sock` |
| `/api/inventory` | `pifridge_inventory` via `/var/run/pifridge/pifridge_inventory.sock` |
| `/api/inventory/delete` | `pifridge_inventory` (listed before `/api/inventory` so nginx matches it first) |
| `/api/inventory/decrement` | `pifridge_inventory` |
| `/api/inventory/increment` | `pifridge_inventory` |

> **Note:** `/api/inventory/delete` must appear before `/api/inventory` in the config. nginx matches `location` blocks in order of specificity — a more specific prefix listed first ensures delete requests are not caught by the general `/api/inventory` block.



## Automatic Setup (recommended)

`run.sh` handles copying the config and reloading nginx automatically. Just run:

```bash
chmod +x run.sh
./run.sh
```


## Manual Setup

If you need to configure nginx by hand:

### 1. Copy the config

```bash
sudo cp config/pifridge.conf /etc/nginx/sites-available/pifridge
```

### 2. Update the `root` path

Open `/etc/nginx/sites-available/pifridge` and update the `root` directive to match where you cloned the repo and where `index.html` is served from:

```nginx
root /var/www/pifridge;
```

Copy `index.html` to that location:

```bash
sudo mkdir -p /var/www/pifridge
sudo cp src/web_app/index.html /var/www/pifridge/
```

### 3. Enable the site

```bash
sudo ln -s /etc/nginx/sites-available/pifridge /etc/nginx/sites-enabled/pifridge
```

You may want to disable the default nginx site to avoid conflicts:

```bash
sudo rm -f /etc/nginx/sites-enabled/default
```

### 4. Create runtime directories

The FastCGI processes write Unix sockets here:

```bash
sudo mkdir -p /var/run/pifridge
sudo chown $USER /var/run/pifridge
```

### 5. Test and reload

```bash
sudo nginx -t
sudo systemctl reload nginx
```



## Dependencies

```bash
sudo apt install nginx
```



## Authors

**David Mead** and **Patrick Dawodu** — nginx configuration, FastCGI socket routing, and `run.sh` integration.


## Acknowledgements

FastCGI and nginx integration follows Dr. Bernd Porr's [*Realtime Embedded Coding in C++ under Linux*](https://berndporr.github.io/realtime_cpp_coding/).
