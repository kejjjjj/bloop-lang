The bloop reference's sources: one plain text file per page. docs/build.bloop turns them into html:

    bloop docs/build.bloop                     (docs/src -> docs/html)
    bloop docs/build.bloop <source> <output>

It also writes <output>/llms.txt (every page as one plain text file, for AI agents and other programs) and
<output>/llms-short.txt (the For agents page and every standard library declaration).

A page starts with a header, "key: value" lines up to a line with "---":

    title:    the page's title
    path:     the breadcrumb after "bloop", parts separated by "/" (a part that's a defined term links to it)
    defines:  terms this page defines (comma separated): mentioned anywhere else, they link here
    group:    pages of the same group get a bar of links to each other under the title (language, std)
    order:    the order in that bar (a number, then the title)

The body, blocks separated by blank lines:

    # Heading                      a section (h2); "# Heading {=term, other}" defines terms that link to it
    ## Heading                     a smaller one (h3), {=...} too
    paragraph text                 lines are joined up to a blank line
    - item                         a list (an indented line after an item goes on with it)
    | a | b |                      a table, the first row its header
    decl std.x.y(a, b) -> int      a declaration box (one row per line; "  // text" after it goes to the right)
    param name | what it is        a parameter table
    > text                         a note
    ```                            a code example (bloop, coloured) up to the next ```
    ```output                      what an example prints
    :::Title ... :::               a box (the front page's)

In the text:

    `code`                         code; linked when it's a defined term (or calls one: `std.array.push(a, 1)`)
    ``code with ` in it``          the same, for code that has a backquote
    **bold**
    [[term]], [[term|text]]        a link to a term, or [[page/id|text]] to a page
    {{term}}                       defines the term right there (it links to this spot)

Every defined term links wherever it's mentioned: once per section, not in the section that defines it, the case
ignored ("Closures" is "closure"), an "s" after it allowed. Names with a dot (std.array.push) also link in the
examples.
