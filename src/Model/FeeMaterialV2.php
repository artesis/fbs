<?php

namespace FBS\Model;


class FeeMaterialV2 {

  /**
   * @var string The FAUST number of the bibliographic record
   * @required
   */
  public $recordId = null;

  /**
   * @var MaterialGroup Name of the material group that the material belongs to
   * @required
   */
  public $materialGroup = null;

  /**
   * @var Periodical Present if material is a periodical
   */
  public $periodical = null;

  /**
   * @var string Identifies the exact material covered by the fee
   * @required
   */
  public $materialItemNumber = null;
}
