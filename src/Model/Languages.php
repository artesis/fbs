<?php

namespace FBS\Model;


class Languages {

  /**
   * @var array[string]
   */
  public $language = null;

  function __construct($language) {
    $this->language = $language;
  }

  /**
   * @return array
   */
  public function getLanguage() {
    return $this->language;
  }
}
