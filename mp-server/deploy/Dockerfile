FROM scratch
COPY --chmod=0555 go-anchor /go-anchor
USER 65532:65532
EXPOSE 8080
ENTRYPOINT ["/go-anchor"]
CMD ["-port", "0", "-http", ":8080"]
