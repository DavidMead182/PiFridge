fetch("https://world.openfoodfacts.net/api/v2/product/3274080005003.json", {
  method: "GET",
  headers: { Authorization: "Basic " + btoa("off:off") },
})
  .then((response) => response.json())
  .then((json) => console.log(json));