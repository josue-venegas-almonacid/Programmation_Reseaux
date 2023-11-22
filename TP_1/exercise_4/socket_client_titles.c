/* Client pour les sockets
 */

#include <stdio.h>
#include <curl/curl.h>
#include <string.h>
#include <stdlib.h>

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
  size_t realsize = size * nmemb;
  char *title_start = "<title>";
  char *title_end = "</title>";
  char *title_ptr = NULL;

  // Find the start of the title tag
  title_ptr = strstr(ptr, title_start);
  if (title_ptr != NULL)
  {
    title_ptr += strlen(title_start);

    // Find the end of the title tag
    char *title_end_ptr = strstr(title_ptr, title_end);
    if (title_end_ptr != NULL)
    {
      // Calculate the length of the title
      size_t title_length = title_end_ptr - title_ptr;

      // Allocate memory for the title
      char *title = (char *)malloc(title_length + 1);
      if (title != NULL)
      {
        // Copy the title to the allocated memory
        strncpy(title, title_ptr, title_length);
        title[title_length] = '\0';

        // Print the title
        printf("Title: %s\n", title);

        // Free the allocated memory
        free(title);
      }
    }
  }

  return realsize;
}

int main(void)
{
  CURL *curl;
  CURLcode res;

  curl = curl_easy_init();
  if (curl)
  {
    curl_easy_setopt(curl, CURLOPT_URL, "https://www.youtube.com/");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    res = curl_easy_perform(curl);

    if (CURLE_OK == res)
    {
      char *ct;
      /* ask for the content-type */
      res = curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ct);

      if ((CURLE_OK == res) && ct)
        printf("We received Content-Type: %s\n", ct);
    }

    /* always cleanup */
    curl_easy_cleanup(curl);
  }
  return 0;
}
